#include "unicode.h"
#include <stddef.h>
#include "unicode_db.h"
#include "unicode_collation.h"
#include "unicode_special_casing.h"
#include "unicode_exclusions.h"
#include "unicode_derived_normalization_props.h"

static int unicode_find_index(uint32_t cp, size_t *index) {
    size_t left = 0;
    size_t right = unicode_skip_len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        uint32_t start = unicode_db[unicode_skip[mid]].code;
        if (cp < start) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    if (left == 0)
        return 0;
    size_t start_idx = unicode_skip[left - 1];
    uint32_t start_code = unicode_db[start_idx].code;
    size_t end_idx = (left < unicode_skip_len) ? unicode_skip[left] : unicode_db_len;
    size_t offset = cp - start_code;
    if (offset >= end_idx - start_idx)
        return 0;
    *index = start_idx + offset;
    if (unicode_db[*index].code != cp)
        return 0;
    return 1;
}

static int unicode_collation_find_index(uint32_t cp, size_t *index) {
    for (size_t i = 0; i < unicode_collation_skip_len; ++i) {
        size_t start_idx = unicode_collation_skip[i];
        uint32_t start_code = unicode_collation_db[start_idx].code;
        size_t end_idx =
            (i + 1 < unicode_collation_skip_len) ? unicode_collation_skip[i + 1]
                                               : unicode_collation_db_len;
        size_t block_len = end_idx - start_idx;

        if (cp < start_code)
            return 0;

        if ((uint32_t)(cp - start_code) < block_len) {
            *index = start_idx + (cp - start_code);
            if (unicode_collation_db[*index].code == cp)
                return 1;
            return 0;
        }
    }
    return 0;
}

uint32_t unicode_tolower(uint32_t cp) {
    size_t idx;
    if (unicode_find_index(cp, &idx)) {
        if (unicode_db[idx].simple_lower != 0)
            return unicode_db[idx].simple_lower;
    }
    return cp;
}

uint32_t unicode_toupper(uint32_t cp) {
    size_t idx;
    if (unicode_find_index(cp, &idx)) {
        if (unicode_db[idx].simple_upper != 0)
            return unicode_db[idx].simple_upper;
    }
    return cp;
}

size_t unicode_tolower_full(uint32_t cp, uint32_t out[3]) {
    for (size_t i = 0; i < unicode_special_cases_len; i++) {
        if (unicode_special_cases[i].code == cp) {
            for (size_t j = 0; j < unicode_special_cases[i].lower_len; j++)
                out[j] = unicode_special_cases[i].lower[j];
            return unicode_special_cases[i].lower_len;
        }
    }
    out[0] = unicode_tolower(cp);
    return 1;
}

size_t unicode_toupper_full(uint32_t cp, uint32_t out[3]) {
    for (size_t i = 0; i < unicode_special_cases_len; i++) {
        if (unicode_special_cases[i].code == cp) {
            for (size_t j = 0; j < unicode_special_cases[i].upper_len; j++)
                out[j] = unicode_special_cases[i].upper[j];
            return unicode_special_cases[i].upper_len;
        }
    }
    out[0] = unicode_toupper(cp);
    return 1;
}

static uint8_t unicode_locale_mask(const char *locale) {
    if (!locale || !locale[0])
        return 0;
    if ((locale[0] == 't' || locale[0] == 'T') && (locale[1] == 'r' || locale[1] == 'R'))
        return UNICODE_LOCALE_TURKIC;
    if ((locale[0] == 'a' || locale[0] == 'A') && (locale[1] == 'z' || locale[1] == 'Z'))
        return UNICODE_LOCALE_TURKIC;
    if ((locale[0] == 'l' || locale[0] == 'L') && (locale[1] == 't' || locale[1] == 'T'))
        return UNICODE_LOCALE_LITHUANIAN;
    return 0;
}

size_t unicode_tolower_full_locale(uint32_t cp, const char *locale, uint32_t out[3]) {
    uint8_t mask = unicode_locale_mask(locale);
    if (mask) {
        for (size_t i = 0; i < unicode_special_cases_locale_len; i++) {
            if (unicode_special_cases_locale[i].code == cp &&
                (unicode_special_cases_locale[i].locale & mask)) {
                for (size_t j = 0; j < unicode_special_cases_locale[i].lower_len; j++)
                    out[j] = unicode_special_cases_locale[i].lower[j];
                return unicode_special_cases_locale[i].lower_len;
            }
        }
    }
    return unicode_tolower_full(cp, out);
}

size_t unicode_toupper_full_locale(uint32_t cp, const char *locale, uint32_t out[3]) {
    uint8_t mask = unicode_locale_mask(locale);
    if (mask) {
        for (size_t i = 0; i < unicode_special_cases_locale_len; i++) {
            if (unicode_special_cases_locale[i].code == cp &&
                (unicode_special_cases_locale[i].locale & mask)) {
                for (size_t j = 0; j < unicode_special_cases_locale[i].upper_len; j++)
                    out[j] = unicode_special_cases_locale[i].upper[j];
                return unicode_special_cases_locale[i].upper_len;
            }
        }
    }
    return unicode_toupper_full(cp, out);
}

int unicode_collation_lookup(uint32_t cp, uint16_t *primary, uint16_t *secondary, uint16_t *tertiary) {
    size_t idx;
    if (unicode_collation_find_index(cp, &idx)) {
        if (primary)
            *primary = unicode_collation_db[idx].primary;
        if (secondary)
            *secondary = unicode_collation_db[idx].secondary;
        if (tertiary)
            *tertiary = unicode_collation_db[idx].tertiary;
        return 1;
    }
    return 0;
}

static int unicode_uint32_list_contains(const uint32_t *arr, size_t len, uint32_t cp) {
    size_t left = 0;
    size_t right = len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (arr[mid] == cp)
            return 1;
        if (cp < arr[mid])
            right = mid;
        else
            left = mid + 1;
    }
    return 0;
}

static int unicode_qc_lookup(const unicode_qc_range_t *ranges, size_t len, uint32_t cp) {
    size_t left = 0;
    size_t right = len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (cp < ranges[mid].start) {
            right = mid;
        } else if (cp > ranges[mid].end) {
            left = mid + 1;
        } else {
            return ranges[mid].qc;
        }
    }
    return UNICODE_QC_YES;
}

static int unicode_range_contains(const unicode_range_t *ranges, size_t len, uint32_t cp) {
    size_t left = 0;
    size_t right = len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (cp < ranges[mid].start) {
            right = mid;
        } else if (cp > ranges[mid].end) {
            left = mid + 1;
        } else {
            return 1;
        }
    }
    return 0;
}

static int unicode_nfkc_cf_find(uint32_t cp, size_t *index, uint8_t *len) {
    size_t left = 0;
    size_t right = unicode_nfkc_cf_map_len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (cp < unicode_nfkc_cf_map[mid].code) {
            right = mid;
        } else if (cp > unicode_nfkc_cf_map[mid].code) {
            left = mid + 1;
        } else {
            *index = unicode_nfkc_cf_map[mid].index;
            *len = unicode_nfkc_cf_map[mid].len;
            return 1;
        }
    }
    return 0;
}

static int unicode_nfkc_scf_find(uint32_t cp, size_t *index, uint8_t *len) {
    size_t left = 0;
    size_t right = unicode_nfkc_scf_map_len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (cp < unicode_nfkc_scf_map[mid].code) {
            right = mid;
        } else if (cp > unicode_nfkc_scf_map[mid].code) {
            left = mid + 1;
        } else {
            *index = unicode_nfkc_scf_map[mid].index;
            *len = unicode_nfkc_scf_map[mid].len;
            return 1;
        }
    }
    return 0;
}

int unicode_is_composition_exclusion(uint32_t cp) {
    return unicode_uint32_list_contains(unicode_exclusions, unicode_exclusions_len, cp);
}

int unicode_get_nfd_qc(uint32_t cp) { return unicode_qc_lookup(unicode_nfd_qc, unicode_nfd_qc_len, cp); }
int unicode_get_nfc_qc(uint32_t cp) { return unicode_qc_lookup(unicode_nfc_qc, unicode_nfc_qc_len, cp); }
int unicode_get_nfkd_qc(uint32_t cp) { return unicode_qc_lookup(unicode_nfkd_qc, unicode_nfkd_qc_len, cp); }
int unicode_get_nfkc_qc(uint32_t cp) { return unicode_qc_lookup(unicode_nfkc_qc, unicode_nfkc_qc_len, cp); }

size_t unicode_nfkc_cf(uint32_t cp, uint32_t out[UNICODE_NFKC_CF_MAX]) {
    size_t index;
    uint8_t len;
    if (unicode_nfkc_cf_find(cp, &index, &len)) {
        for (size_t i = 0; i < len; i++)
            out[i] = unicode_nfkc_cf_data[index + i];
        return len;
    }
    out[0] = cp;
    return 1;
}

size_t unicode_nfkc_scf(uint32_t cp, uint32_t out[UNICODE_NFKC_SCF_MAX]) {
    size_t index;
    uint8_t len;
    if (unicode_nfkc_scf_find(cp, &index, &len)) {
        for (size_t i = 0; i < len; i++)
            out[i] = unicode_nfkc_scf_data[index + i];
        return len;
    }
    out[0] = cp;
    return 1;
}

int unicode_changes_when_nfkc_casefolded(uint32_t cp) {
    return unicode_range_contains(unicode_cwkcf, unicode_cwkcf_len, cp);
}

size_t unicode_normalize(uint32_t *buf, size_t len, size_t cap) {
    (void)cap;
    /* TODO: full Unicode normalization (NFC) */
    return len;
}

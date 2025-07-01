#include "unicode.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
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

/* helper: get canonical combining class */
static int unicode_combining_class(uint32_t cp) {
    size_t idx;
    if (unicode_find_index(cp, &idx)) {
        return atoi(unicode_db[idx].combining_class);
    }
    return 0;
}

/* parse decomposition string as sequence of code points
   returns number of code points parsed, or 0 if none or compatibility */
static size_t unicode_parse_decomp(const char *str, uint32_t *out, size_t cap) {
    if (!str || !str[0])
        return 0;
    if (str[0] == '<')
        return 0; /* compatibility mapping */
    size_t n = 0;
    const char *p = str;
    while (*p && n < cap) {
        while (*p == ' ')
            p++;
        char *end;
        unsigned long v = strtoul(p, &end, 16);
        if (end == p)
            break;
        out[n++] = (uint32_t)v;
        p = end;
        while (*p == ' ')
            p++;
    }
    return n;
}

/* canonical decomposition of single code point */
static size_t unicode_decompose_char(uint32_t cp, uint32_t *out, size_t cap) {
    /* Hangul decomposition */
    const uint32_t SBase = 0xAC00;
    const uint32_t LBase = 0x1100;
    const uint32_t VBase = 0x1161;
    const uint32_t TBase = 0x11A7;
    const int LCount = 19;
    const int VCount = 21;
    const int TCount = 28;
    const int NCount = VCount * TCount;
    const int SCount = LCount * NCount;

    if (cp >= SBase && cp < SBase + SCount) {
        int SIndex = cp - SBase;
        uint32_t L = LBase + SIndex / NCount;
        uint32_t V = VBase + (SIndex % NCount) / TCount;
        uint32_t T = TBase + SIndex % TCount;
        out[0] = L;
        out[1] = V;
        if (T != TBase && cap >= 3) {
            out[2] = T;
            return 3;
        }
        return 2;
    }

    size_t idx;
    if (unicode_find_index(cp, &idx)) {
        uint32_t tmp[18];
        size_t n = unicode_parse_decomp(unicode_db[idx].decomposition, tmp, 18);
        if (n > 0 && n <= cap) {
            for (size_t i = 0; i < n; i++)
                out[i] = tmp[i];
            return n;
        }
    }
    if (cap > 0)
        out[0] = cp;
    return 1;
}

/* recursive canonical decomposition */
static size_t unicode_decompose(uint32_t cp, uint32_t *out, size_t cap) {
    uint32_t tmp[18];
    size_t n = unicode_decompose_char(cp, tmp, 18);
    size_t total = 0;
    for (size_t i = 0; i < n && total < cap; i++) {
        if (tmp[i] == cp && n == 1) {
            out[total++] = tmp[i];
        } else {
            total += unicode_decompose(tmp[i], out + total, cap - total);
        }
    }
    return total;
}

/* lookup composition of pair */
static int unicode_compose_pair(uint32_t a, uint32_t b, uint32_t *out) {
    const uint32_t SBase = 0xAC00;
    const uint32_t LBase = 0x1100;
    const uint32_t VBase = 0x1161;
    const uint32_t TBase = 0x11A7;
    const int LCount = 19;
    const int VCount = 21;
    const int TCount = 28;
    const int NCount = VCount * TCount;
    const int SCount = LCount * NCount;

    if (LBase <= a && a < LBase + LCount && VBase <= b && b < VBase + VCount) {
        int LIndex = a - LBase;
        int VIndex = b - VBase;
        *out = SBase + (LIndex * VCount + VIndex) * TCount;
        return 1;
    }
    if (SBase <= a && a < SBase + SCount && (a - SBase) % TCount == 0 &&
        TBase < b && b < TBase + TCount) {
        int TIndex = b - TBase;
        *out = a + TIndex;
        return 1;
    }

    uint32_t seq[3];
    for (size_t i = 0; i < unicode_db_len; i++) {
        size_t n = unicode_parse_decomp(unicode_db[i].decomposition, seq, 3);
        if (n == 2 && seq[0] == a && seq[1] == b) {
            uint32_t cp = unicode_db[i].code;
            if (!unicode_is_composition_exclusion(cp)) {
                *out = cp;
                return 1;
            }
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
    if (!buf)
        return 0;

    uint32_t *tmp = (uint32_t *)malloc(sizeof(uint32_t) * cap);
    if (!tmp)
        return len;

    /* Canonical decomposition with ordering */
    size_t out_len = 0;
    size_t segment_start = 0;
    for (size_t i = 0; i < len && out_len < cap; i++) {
        uint32_t dec[18];
        size_t dlen = unicode_decompose(buf[i], dec, 18);
        for (size_t j = 0; j < dlen && out_len < cap; j++) {
            uint32_t c = dec[j];
            int cc = unicode_combining_class(c);
            if (cc == 0) {
                segment_start = out_len;
                tmp[out_len++] = c;
            } else {
                size_t k = out_len;
                while (k > segment_start && unicode_combining_class(tmp[k - 1]) > cc) {
                    tmp[k] = tmp[k - 1];
                    k--;
                }
                tmp[k] = c;
                out_len++;
            }
        }
    }

    /* Canonical composition */
    size_t starter = 0;
    int prev_cc = 0;
    for (size_t i = 1; i < out_len; i++) {
        int cc = unicode_combining_class(tmp[i]);
        uint32_t comp;
        if (cc != 0 && prev_cc < cc && unicode_compose_pair(tmp[starter], tmp[i], &comp)) {
            tmp[starter] = comp;
            memmove(tmp + i, tmp + i + 1, (out_len - i - 1) * sizeof(uint32_t));
            out_len--;
            i--; /* re-evaluate at same index */
            prev_cc = 0;
            continue;
        }
        if (cc == 0) {
            starter = i;
            prev_cc = 0;
        } else {
            prev_cc = cc;
        }
    }

    if (out_len > cap)
        out_len = cap;
    memcpy(buf, tmp, out_len * sizeof(uint32_t));
    free(tmp);
    return out_len;
}

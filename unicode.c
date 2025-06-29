#include "unicode.h"
#include <stddef.h>
#include "unicode_db.h"
#include "unicode_collation.h"

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

#include "unicode.h"
#include <stddef.h>
#include "unicode_db.h"

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

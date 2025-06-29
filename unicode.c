#include "unicode.h"
#include "unicode_case_data.h"
#include <stddef.h>

uint32_t unicode_tolower(uint32_t cp) {
    for (size_t i = 0; i < unicode_tolower_map_len; i++) {
        if (unicode_tolower_map[i].from == cp) {
            return unicode_tolower_map[i].to;
        }
    }
    if (cp >= 'A' && cp <= 'Z') {
        return cp + 32;
    }
    return cp;
}

uint32_t unicode_toupper(uint32_t cp) {
    for (size_t i = 0; i < unicode_toupper_map_len; i++) {
        if (unicode_toupper_map[i].from == cp) {
            return unicode_toupper_map[i].to;
        }
    }
    if (cp >= 'a' && cp <= 'z') {
        return cp - 32;
    }
    return cp;
}

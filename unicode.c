#include "unicode.h"
#include "unicode_db.h"
#include <stddef.h>

uint32_t unicode_tolower(uint32_t cp) {
    size_t left = 0;
    size_t right = unicode_db_len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        uint32_t code = unicode_db[mid].code;
        if (code == cp) {
            if (unicode_db[mid].simple_lower != 0) {
                return unicode_db[mid].simple_lower;
            }
            break;
        }
        if (cp < code) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    return cp;
}

uint32_t unicode_toupper(uint32_t cp) {
    size_t left = 0;
    size_t right = unicode_db_len;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        uint32_t code = unicode_db[mid].code;
        if (code == cp) {
            if (unicode_db[mid].simple_upper != 0) {
                return unicode_db[mid].simple_upper;
            }
            break;
        }
        if (cp < code) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }
    return cp;
}

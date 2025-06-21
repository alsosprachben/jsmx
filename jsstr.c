#include <string.h>
#include <errno.h>

#include "jsstr.h"
#include "utf8.h"

size_t jsstr_head_size() {
    return sizeof(jsstr32_t);
}

void jsstr_init(jsstr32_t *s) {
    s->cap = 0;
    s->len = 0;
    s->codepoints = NULL;
}

void jsstr32_init_from_buf(jsstr32_t *s, const char *buf, size_t len) {
    s->cap = len / sizeof(uint32_t);
    s->len = 0;
    s->codepoints = (uint32_t *) buf;
}

void jsstr32_slice(jsstr32_t *s, jsstr32_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr32_wstr_codepoint_at() to slice the buffer */
    s->codepoints = jsstr32_wstr_codepoint_at(src, start_i);
    s->len = src->codepoints + src->len - s->codepoints;
    if (stop_i >= 0) {
        jsstr32_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr32_cmp(jsstr32_t *s1, jsstr32_t *s2) {
    if (s1->len != s2->len) {
        return s1->len - s2->len;
    }
    for (size_t i = 0; i < s1->len; i++) {
        if (s1->codepoints[i] != s2->codepoints[i]) {
            return s1->codepoints[i] - s2->codepoints[i];
        }
    }
    return 0;
}

ssize_t jsstr32_indexof(jsstr32_t *s, uint32_t c, size_t start_i) {
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && s->codepoints[i] == c) {
            return i;
        }
    }
    return -i;
}

ssize_t jsstr32_indextoken(jsstr32_t *s, uint32_t *c, size_t c_len, size_t start_i) {
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && i + c_len <= s->len) {
            int j;
            for (j = 0; j < c_len; j++) {
                if (s->codepoints[i] == c[j]) {
                    return i;
                }
            }
        }
    }
    return -i;
}

size_t jsstr32_get_cap(jsstr32_t *s) {
    return s->cap;
}

size_t jsstr32_set_from_utf32(jsstr32_t *s, const uint32_t *str, size_t len) {
    /* copy the wide characters into the codepoints array */
    if (len > s->cap) {
        errno = ENOBUFS; /* not enough capacity */
        return 0;
    }
    memcpy(s->codepoints, str, len * sizeof(uint32_t));
    s->len = len;
    return s->len;
}

size_t jsstr32_set_from_utf16(jsstr32_t *s, const uint16_t *str, size_t len) {
    /* decode each UTF-16 code unit into a code point elements, assigning into codepoints array */
    const uint16_t *bc = str;
    const uint16_t *bs = str + len;
    uint32_t *cc = s->codepoints;
    uint32_t *cs = s->codepoints + s->cap;
    UTF16_DECODE((const uint16_t **) &bc, (const uint16_t *) bs, &cc, cs);
    s->len = cc - s->codepoints;
    return s->len;
}

size_t jsstr32_set_from_utf8(jsstr32_t *s, uint8_t *str, size_t len) {
    /* decode each utf-8 sequence into a code point elements, assigning into codepoints array */
    uint8_t *bc = str;
    uint8_t *bs = str + len;
    uint32_t *cc = s->codepoints;
    uint32_t *cs = s->codepoints + s->cap;
    UTF8_DECODE((const char **) &bc, (const char *) bs, &cc, cs);
    s->len = cc - s->codepoints;
    return s->len;
}

jsstr32_t jsstr32_from_str(const uint32_t *str) {
    jsstr32_t s;
    size_t len = wcslen(str);
    jsstr32_init_from_buf(&s, (char *) str, len);
    s.len = len;
    return s;
}

size_t jsstr32_get_utf32len(jsstr32_t *s) {
    return s->len;
}
size_t jsstr32_get_utf16len(jsstr32_t *s) {
    /* determine how many surrogate pairs are in the string to calculate the UTF-16 length */
    size_t utf16len = 0;
    for (size_t i = 0; i < s->len; i++) {
        uint32_t c = s->codepoints[i];
        int l;
        l = UTF16_CLEN(c);
        utf16len += l;
    }
    return utf16len;
}
size_t jsstr32_get_utf8len(jsstr32_t *s) {
    /* determine how many bytes are needed to encode the string in UTF-8 */
    /* use UTF8_CLEN(c, l) macro from utf8.h */
    size_t utf8len = 0;
    for (size_t i = 0; i < s->len; i++) {
        uint32_t c = s->codepoints[i];
        int l;
        l = UTF8_CLEN(c);
        utf8len += l;
    }
    return utf8len;
}

uint32_t *jsstr32_wstr_codepoint_at(jsstr32_t *s, size_t i) {
    if (i < s->len) {
        return s->codepoints + i;
    } else {
        return NULL;
    }
}

void jsstr32_truncate(jsstr32_t *s, size_t len) {
    uint32_t *c = jsstr32_wstr_codepoint_at(s, len);
    if (c != NULL) {
        s->len = c - s->codepoints;
    }
}

jsstr32_t jsstr32_jsstr_codepoint_at2(jsstr32_t *s, ssize_t index) {
    if (index < 0) {
        index += s->len; /* convert negative index to positive */
    }
    return jsstr32_jsstr_codepoint_at(s, index);
}

jsstr32_t jsstr32_jsstr_codepoint_at(jsstr32_t *s, ssize_t index) {
    /* jsstr32_slice() to slice using the s string's buffer */
    jsstr32_t result;
    jsstr_init(&result);
    if (index < 0 || index >= s->len) {
        /* out of bounds, return empty string */
        return result;
    }
    jsstr32_slice(&result, s, index, index + 1);
    return result;
}

uint16_t jsstr32_u16_codeunit_at(jsstr32_t *s, ssize_t index, const char *buf, size_t len) {
    /* make a jsstr16_t string of this string, to index code units instead of code points */
    size_t rc;
    jsstr16_t jsstr16;
    jsstr16_init_from_buf(&jsstr16, buf, len);
    rc = jsstr16_set_from_utf32(&jsstr16, s->codepoints, s->len);
    if (rc < index) {
        errno = ENOBUFS; /* not enough capacity */
        return 0; /* out of bounds */
    }
    return jsstr16_u16_codeunit_at(&jsstr16, index);
}

uint32_t jsstr32_u32_codepoint_at(jsstr32_t *s, ssize_t index) {
    /* return the code point at the index, or 0 if out of bounds */
    if (index < 0 || index >= s->len) {
        errno = ENOBUFS; /* not enough capacity */
        return 0; /* out of bounds */
    }
    return s->codepoints[index];
}

int jsstr32_concat(jsstr32_t *s, jsstr32_t *src) {
    /* concatenate src to s, if there is enough capacity */
    if (s->len + src->len > s->cap) {
        errno = ENOBUFS;
        return -1; /* not enough capacity */
    }
    memcpy(s->codepoints + s->len, src->codepoints, src->len * sizeof(uint32_t));
    s->len += src->len;
    return 0; /* success */
}

void jsstr16_init(jsstr16_t *s) {
    s->cap = 0;
    s->len = 0;
    s->codeunits = NULL;
}

void jsstr16_init_from_buf(jsstr16_t *s, const char *buf, size_t len) {
    s->cap = len / sizeof(uint16_t);
    s->len = 0;
    s->codeunits = (uint16_t *) buf;
}

void jsstr16_slice(jsstr16_t *s, jsstr16_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr16_u16s_codeunit_at() to slice the buffer */
    s->codeunits = jsstr16_u16s_codeunit_at(src, start_i);
    s->len = src->codeunits + src->len - s->codeunits;
    if (stop_i >= 0) {
        jsstr16_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr16_cmp(jsstr16_t *s1, jsstr16_t *s2) {
    if (s1->len != s2->len) {
        return s1->len - s2->len;
    }
    for (size_t i = 0; i < s1->len; i++) {
        if (s1->codeunits[i] != s2->codeunits[i]) {
            return s1->codeunits[i] - s2->codeunits[i];
        }
    }
    return 0;
}

ssize_t jsstr16_indexof(jsstr16_t *s, uint32_t search_c, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* account for surrogate pairs */
    uint32_t c;
    ssize_t c_i;
    size_t i;
    for (i = 0, c_i = 0; i < s->len; i++, c_i++) {
        if (s->codeunits[i] >= 0xD800 && s->codeunits[i] <= 0xDBFF) {
            /* detected high surrogate */
            if (i + 1 < s->len && s->codeunits[i + 1] >= 0xDC00 && s->codeunits[i + 1] <= 0xDFFF) {
                /* detected low surrogate */
                c = 0x10000 + ((s->codeunits[i] - 0xD800) << 10) + (s->codeunits[i + 1] - 0xDC00);
                i++;
            } else {
                c = 0xFFFD; /* the replacement character */
            }
        } else {
            c = s->codeunits[i];            
        }
        if (c_i >= start_i && search_c == c) {
            return c_i;
        }
    }
    return -c_i;
}

ssize_t jsstr16_indextoken(jsstr16_t *s, uint32_t *search_c, size_t c_len, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* account for surrogate pairs */
    uint32_t c;
    ssize_t c_i;
    size_t i;
    for (i = 0, c_i = 0; i < s->len; i++, c_i++) {
        if (s->codeunits[i] >= 0xD800 && s->codeunits[i] <= 0xDBFF) {
            /* detected high surrogate */
            if (i + 1 < s->len && s->codeunits[i + 1] >= 0xDC00 && s->codeunits[i + 1] <= 0xDFFF) {
                /* detected low surrogate */
                c = 0x10000 + ((s->codeunits[i] - 0xD800) << 10) + (s->codeunits[i + 1] - 0xDC00);
                i += 1;
            } else {
                c = 0xFFFD; /* the replacement character */
            }
        } else {
            c = s->codeunits[i];            
        }
        if (c_i >= start_i) {
            int j;
            for (j = 0; j < c_len; j++) {
                if (search_c[j] == c) {
                    return c_i;
                }
            }
        }
    }
    return -c_i;
}

size_t jsstr16_get_cap(jsstr16_t *s) {
    return s->cap;
}

size_t jsstr16_set_from_utf32(jsstr16_t *s, const uint32_t *str, size_t len) {
    /* convert a wide character string to a UTF-16 code unit string */
    size_t i = 0;
    size_t j = 0;
    while (i < len && j < s->cap) {
        uint32_t c = str[i];
        if (c >= 0x10000) {
            if (j + 1 < s->cap) {
                UTF16_CODEPAIR(c, s->codeunits + j);
                j += 2; /* surrogate pair */
            } else {
                break; /* not enough capacity */
            }
        } else {
            s->codeunits[j] = c;
            j++;
        }
        i++;
    }
    s->len = j;
    return i;
}

size_t jsstr16_set_from_utf16(jsstr16_t *s, const uint16_t *str, size_t len) {
    /* copy the UTF-16 code units directly into the codeunits array */
    size_t i = 0;
    size_t j = 0;
    while (i < len && j < s->cap) {
        s->codeunits[j] = str[i];
        j++;
        i++;
    }
    s->len = j;
    return j;
}

size_t jsstr16_set_from_utf8(jsstr16_t *s, const uint8_t *str, size_t len) {
    /* decode each utf-8 sequence into a code point elements, assigning into codeunits array */
    size_t i = 0;
    size_t j = 0;
    while (i < len && i < s->cap) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *) str + i, (const char *) str + len, &c, &l);
        if (l > 0) {
            if (c >= 0x10000) {
                s->codeunits[j] = 0xD800 + ((c - 0x10000) >> 10);
                j++;
                s->codeunits[j] = 0xDC00 + ((c - 0x10000) & 0x3FF);
                j++;
            } else {
                s->codeunits[j] = c;
                j++;
            }
            i += l;
        } else {
            /* stop at invalid byte sequence */
            break;
        }
    }
    s->len = j;
    return j;
}

size_t str16len(const uint16_t *str) {
    size_t len = 0;
    while (str[len] != 0) {
        len++;
    }
    return len;
}

jsstr16_t jsstr16_from_str(const uint16_t *str) {
    jsstr16_t s;
    size_t len = str16len(str);
    jsstr16_init_from_buf(&s, (char *) str, len);
    s.len = len;
    return s;
}

size_t jsstr16_get_utf32len(jsstr16_t *s) {
    /* determine how many surrogate pairs are in the string to calculate the character length */

    int l;
    size_t code_i;
    size_t char_i;

    for (
        code_i = 0,  char_i = 0, l = 0;
        code_i < s->len;
        code_i += l, char_i++
    ) {
        l = UTF16_BLEN(s->codeunits + code_i, s->codeunits + s->len);
        if (l == 0) {
            errno = EINVAL; /* invalid byte sequence */
            return 0; /* error */
        }
    }
    return char_i;
}

size_t jsstr16_get_utf16len(jsstr16_t *s) {
    return s->len;
}

size_t jsstr16_get_utf8len(jsstr16_t *s) {
    /* count along surrogate pairs, and use the UTF8_CLEN(c, l) macro from utf8.h to determine length */
    /* use UTF16_BLEN() and UTF16_CHAR() to get c, then use UTF8_CLEN() to reduce the utf8len */
    
    uint32_t c;
    int l;
    size_t code16_i;
    size_t code8_i;
    
    for (
        code16_i = 0,  code8_i = 0, l = 0, c = 0;
        code16_i < s->len;
        code16_i += l, code8_i += UTF8_CLEN(c)
    ) {
        UTF16_CHAR(s->codeunits + code16_i, s->codeunits + s->len, &c, &l);
        if (l == 0) {
            errno = EINVAL; /* invalid byte sequence */
            return 0; /* error */
        }
    }

    return code8_i;
}

uint16_t *jsstr16_u16s_codeunit_at(jsstr16_t *s, size_t index) {
    /* i is code point index, so need to count surrogate pairs of two code units as one code point */
    /* need to walk from the beginning */
    int l;
    size_t code_i;
    size_t char_i;

    for (
        code_i = 0,        char_i = 0, l = 0;
        code_i < s->len && char_i < index;
        code_i += l,       char_i++
    ) {
        l = UTF16_BLEN(s->codeunits + code_i, s->codeunits + s->len);
        if (l == 0) {
            errno = ERANGE; /* out of bounds */
            return NULL; /* out of bounds */
        }
    }

    if (char_i == index) {
        /* found the code point at index i */
        return s->codeunits + code_i;
    } else {
        errno = ERANGE; /* out of bounds */
        return NULL; /* out of bounds */
    }
}

void jsstr16_truncate(jsstr16_t *s, size_t len) {
    uint16_t *cu = jsstr16_u16s_codeunit_at(s, len);
    if (cu != NULL) {
        s->len = cu - s->codeunits;
    }
}

jsstr16_t jsstr16_jsstr16_codepoint_at2(jsstr16_t *s, ssize_t index) {
    if (index < 0) {
        index += s->len; /* convert negative index to positive */
    }
    return jsstr16_jsstr16_codepoint_at(s, index);
}

jsstr16_t jsstr16_jsstr16_codepoint_at(jsstr16_t *s, ssize_t index) {
    /* jsstr16_slice() to slice using the s string's buffer */
    jsstr16_t result;
    jsstr16_init(&result);
    if (index < 0 || index >= s->len) {
        /* out of bounds, return empty string */
        return result;
    }
    jsstr16_slice(&result, s, index, index + 1);
    return result;
}

uint16_t jsstr16_u16_codeunit_at(jsstr16_t *s, ssize_t index) {
    /* return the code unit at the index, or 0 if out of bounds */
    if (index < 0 || index >= s->len) {
        errno = ENOBUFS; /* not enough capacity */
        return 0; /* out of bounds */
    }
    return s->codeunits[index];
}

uint32_t jsstr16_u32_codepoint_at(jsstr16_t *s, ssize_t index, const char *buf, size_t len) {
    /* make a jsstr32_t string of this string, to index code points instead of code units */
    size_t rc;
    jsstr32_t jsstr;
    jsstr32_init_from_buf(&jsstr, buf, len);
    rc = jsstr32_set_from_utf16(&jsstr, s->codeunits, s->len);
    if (rc < index) {
        errno = ENOBUFS; /* not enough capacity */
        return 0; /* out of bounds */
    }
    return jsstr32_u32_codepoint_at(&jsstr, index);
}

int jsstr16_concat(jsstr16_t *s, jsstr16_t *src) {
    /* concatenate src to s, if there is enough capacity */
    if (s->len + src->len > s->cap) {
        errno = ENOBUFS;
        return -1; /* not enough capacity */
    }
    memcpy(s->codeunits + s->len, src->codeunits, src->len * sizeof(uint16_t));
    s->len += src->len;
    return 0; /* success */
}

void jsstr8_init(jsstr8_t *s) {
    s->cap = 0;
    s->len = 0;
    s->bytes = NULL;
}

void jsstr8_init_from_buf(jsstr8_t *s, const char *buf, size_t len) {
    s->cap = len / sizeof(uint8_t);
    s->len = 0;
    s->bytes = (uint8_t *) buf;
}

void jsstr8_slice(jsstr8_t *s, jsstr8_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr8_u8s_byte_at() to slice the buffer */
    s->bytes = jsstr8_u8s_byte_at(src, start_i);
    s->len = src->bytes + src->len - s->bytes;
    if (stop_i >= 0) {
        jsstr8_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr8_cmp(jsstr8_t *s1, jsstr8_t *s2) {
    if (s1->len != s2->len) {
        return s1->len - s2->len;
    }
    for (size_t i = 0; i < s1->len; i++) {
        if (s1->bytes[i] != s2->bytes[i]) {
            return s1->bytes[i] - s2->bytes[i];
        }
    }
    return 0;
}

ssize_t jsstr8_indexof(jsstr8_t *s, uint32_t search_c, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* use UTF8_BLEN(bc, l) from utf8.h to determine byte length from character, and advance the cursor that amount */
    uint8_t *cc; /* character cursor */
    uint8_t *cs; /* character stop */
    uint32_t c; /* character */
    int l; /* byte length */
    ssize_t i; /* index */
    for (i = 0; i < s->len; i += l) {
        cc = s->bytes + i;
        cs = s->bytes + s->len;
        UTF8_CHAR((const char *) cc, (const char *) cs, &c, &l);
        if (l < 0) {
            l = 1;
            c = 0xFFFD; /* the replacement character */
        }

        if (i >= start_i && c == search_c) {
            return i;
        }
    }
    return -i;
}
ssize_t jsstr8_indextoken(jsstr8_t *s, uint32_t *search_c, size_t c_len, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* use UTF8_BLEN(bc, l) from utf8.h to determine byte length from character, and advance the cursor that amount */
    uint8_t *cc; /* character cursor */
    uint8_t *cs; /* character stop */
    uint32_t c; /* character */
    int l; /* byte length */
    ssize_t i; /* index */
    for (i = 0; i < s->len; i += l) {
        cc = s->bytes + i;
        cs = s->bytes + s->len;
        UTF8_CHAR((const char *) cc, (const char *) cs, &c, &l);
        if (l < 0) {
            l = 1;
            c = 0xFFFD; /* the replacement character */
        }

        if (i >= start_i) {
            int j;
            for (j = 0; j < c_len; j++) {
                if (c == search_c[j]) {
                    return i;
                }
            }
        }

    }
    return -i;
}

size_t jsstr8_get_cap(jsstr8_t *s) {
    return s->cap;
}

size_t jsstr8_set_from_utf8(jsstr8_t *s, const uint8_t *str, size_t len) {
    /* copy the utf-8 byte sequence into the bytes array */
    size_t copy_len = len < s->cap ? len : s->cap;
    memcpy(s->bytes, str, copy_len);
    s->len = copy_len;
    return copy_len;
}

jsstr8_t jsstr8_from_str(const char *str) {
    jsstr8_t s;
    size_t len = strlen(str);
    jsstr8_init_from_buf(&s, str, len);
    s.len = len;
    return s;
}

size_t jsstr8_get_utf32len(jsstr8_t *s) {
    /* determine how many bytes are in the string to calculate the character length */
    /* use UTF8_BLEN(bc, l) from utf8.h to determine byte length from character, and advance the cursor that amount */
    size_t charlen = 0;
    for (size_t i = 0; i < s->len; i++) {
        int l;
        UTF8_BLEN((const char *) s->bytes + i, &l);
        i += l;
        charlen += 1;
    }
    return charlen;
}

size_t jsstr8_get_utf16len(jsstr8_t *s) {
    /* determine how many bytes are in the string to calculate the UTF-16 length */
    /* use UTF8_CHAR(cc, cs, c, l) from utf8.h to determine the character from the byte cursor */
    size_t utf16len = 0;
    uint8_t *cc; /* character cursor */
    uint8_t *cs; /* character stop */
    uint32_t c; /* character */
    int l; /* byte length */
    for (size_t i = 0; i < s->len; i++) {
        cc = s->bytes + i;
        cs = s->bytes + s->len;
        UTF8_CHAR((const char *) cc, (const char *) cs, &c, &l);
        if (l > 0) {
            if (c >= 0x10000) {
                utf16len += 2;
            } else {
                utf16len += 1;
            }
        } else {
            /* stop at invalid byte sequence */
            break;
        }
    }
    return utf16len;
}

size_t jsstr8_get_utf8len(jsstr8_t *s) {
    return s->len;
}

uint8_t *jsstr8_u8s_byte_at(jsstr8_t *s, size_t i) {
    /* i is code point index, so need to count each UTF-8 sequence as one code point */
    /* need to walk from the beginning */
    size_t j = 0;
    size_t k = 0;
    for (j = 0, k = 0; j < s->len && k < i; k++, j++) {
        int l;
        UTF8_BLEN((const char *) s->bytes + j, &l);
        j += l;
    }
    if (j < s->len) {
        return s->bytes + j;
    } else {
        return NULL;
    }
}

void jsstr8_truncate(jsstr8_t *s, size_t len) {
    uint8_t *bc = jsstr8_u8s_byte_at(s, len);
    if (bc != NULL) {
        s->len = bc - s->bytes;
    }
}

jsstr8_t jsstr8_jsstr8_codepoint_at2(jsstr8_t *s, ssize_t index) {
    if (index < 0) {
        index += s->len; /* convert negative index to positive */
    }
    return jsstr8_jsstr8_codepoint_at(s, index);
}

jsstr8_t jsstr8_jsstr8_codepoint_at(jsstr8_t *s, ssize_t index) {
    /* jsstr8_slice() to slice using the s string's buffer */
    jsstr8_t result;
    jsstr8_init(&result);
    if (index < 0 || index >= s->len) {
        /* out of bounds, return empty string */
        return result;
    }
    jsstr8_slice(&result, s, index, index + 1);
    return result;
}

uint16_t jsstr8_u16_codeunit_at(jsstr8_t *s, ssize_t index, const char *buf, size_t len) {
    /* make a jsstr16_t string of this string, to index code units instead of code points */
    size_t rc;
    jsstr16_t jsstr16;
    jsstr16_init_from_buf(&jsstr16, buf, len);
    rc = jsstr16_set_from_utf8(&jsstr16, s->bytes, s->len);
    if (rc < index) {
        errno = ENOBUFS; /* not enough capacity */
        return 0; /* out of bounds */
    }
    return jsstr16_u16_codeunit_at(&jsstr16, index);
}

uint32_t jsstr8_u32_codepoint_at(jsstr8_t *s, ssize_t index, const char *buf, size_t len) {
    /* make a jsstr32_t string of this string, to index code points instead of code units */
    size_t rc;
    jsstr32_t jsstr;
    jsstr32_init_from_buf(&jsstr, buf, len);
    rc = jsstr32_set_from_utf8(&jsstr, s->bytes, s->len);
    if (rc < index) {
        errno = ENOBUFS; /* not enough capacity */
        return 0; /* out of bounds */
    }
    return jsstr32_u32_codepoint_at(&jsstr, index);
}

int jsstr8_concat(jsstr8_t *s, jsstr8_t *src) {
    /* concatenate src to s, if there is enough capacity */
    if (s->len + src->len > s->cap) {
        errno = ENOBUFS;
        return -1; /* not enough capacity */
    }
    memcpy(s->bytes + s->len, src->bytes, src->len);
    s->len += src->len;
    return 0; /* success */
}
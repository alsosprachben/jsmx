#include <string.h>
#include <errno.h>

#include "jsstr.h"
#include "utf8.h"

size_t jsstr_head_size() {
    return sizeof(jsstr_t);
}

void jsstr_init(jsstr_t *s) {
    s->cap = 0;
    s->len = 0;
    s->codepoints = NULL;
}

void jsstr_init_from_buf(jsstr_t *s, const char *buf, size_t len) {
    s->cap = len / sizeof(wchar_t);
    s->len = 0;
    s->codepoints = (wchar_t *) buf;
}

void jsstr_slice(jsstr_t *s, jsstr_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr_get_at() to slice the buffer */
    s->codepoints = jsstr_get_at(src, start_i);
    s->len = src->codepoints + src->len - s->codepoints;
    if (stop_i >= 0) {
        jsstr_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr_cmp(jsstr_t *s1, jsstr_t *s2) {
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

ssize_t jsstr_indexof(jsstr_t *s, wchar_t c, size_t start_i) {
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && s->codepoints[i] == c) {
            return i;
        }
    }
    return -i;
}

ssize_t jsstr_indextoken(jsstr_t *s, wchar_t *c, size_t c_len, size_t start_i) {
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

size_t jsstr_get_cap(jsstr_t *s) {
    return s->cap;
}

size_t jsstr_set_from_utf8(jsstr_t *s, uint8_t *str, size_t len) {
    /* decode each utf-8 sequence into a code point elements, assigning into codepoints array */
    uint8_t *bc = str;
    uint8_t *bs = str + len;
    wchar_t *cc = s->codepoints;
    wchar_t *cs = s->codepoints + s->cap;
    UTF8_DECODE((const char **) &bc, (const char *) bs, &cc, cs);
    s->len = cc - s->codepoints;
    return s->len;
}

jsstr_t jsstr_from_str(const wchar_t *str) {
    jsstr_t s;
    size_t len = wcslen(str);
    jsstr_init_from_buf(&s, (char *) str, len);
    s.len = len;
    return s;
}

size_t jsstr_get_charlen(jsstr_t *s) {
    return s->len;
}
size_t jsstr_get_utf16len(jsstr_t *s) {
    /* determine how many surrogate pairs are in the string to calculate the UTF-16 length */
    size_t utf16len = 0;
    for (size_t i = 0; i < s->len; i++) {
        if (s->codepoints[i] >= 0x10000) {
            utf16len += 2;
        } else {
            utf16len += 1;
        }
    }
    return utf16len;
}
size_t jsstr_get_utf8len(jsstr_t *s) {
    /* determine how many bytes are needed to encode the string in UTF-8 */
    /* use UTF8_CLEN(c, l) macro from utf8.h */
    size_t utf8len = 0;
    for (size_t i = 0; i < s->len; i++) {
        wchar_t c = s->codepoints[i];
        int l;
        l = UTF8_CLEN(c);
        utf8len += l;
    }
    return utf8len;
}

wchar_t *jsstr_get_at(jsstr_t *s, size_t i) {
    if (i < s->len) {
        return s->codepoints + i;
    } else {
        return NULL;
    }
}

void jsstr_truncate(jsstr_t *s, size_t len) {
    wchar_t *c = jsstr_get_at(s, len);
    if (c != NULL) {
        s->len = c - s->codepoints;
    }
}

int jsstr_concat(jsstr_t *s, jsstr_t *src) {
    /* concatenate src to s, if there is enough capacity */
    if (s->len + src->len > s->cap) {
        errno = ENOBUFS;
        return -1; /* not enough capacity */
    }
    memcpy(s->codepoints + s->len, src->codepoints, src->len * sizeof(wchar_t));
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
    /* initialize from a slice of a source string, using jsstr16_get_at() to slice the buffer */
    s->codeunits = jsstr16_get_at(src, start_i);
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

ssize_t jsstr16_indexof(jsstr16_t *s, wchar_t search_c, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* account for surrogate pairs */
    wchar_t c;
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

ssize_t jsstr16_indextoken(jsstr16_t *s, wchar_t *search_c, size_t c_len, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* account for surrogate pairs */
    wchar_t c;
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

size_t jsstr16_set_from_utf8(jsstr16_t *s, uint8_t *str, size_t len) {
    /* decode each utf-8 sequence into a code point elements, assigning into codeunits array */
    size_t i = 0;
    size_t j = 0;
    while (i < len && i < s->cap) {
        wchar_t c;
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

size_t jsstr16_get_charlen(jsstr16_t *s) {
    /* determine how many surrogate pairs are in the string to calculate the character length */
    size_t charlen = 0;
    for (size_t i = 0; i < s->len; i++) {
        if (s->codeunits[i] >= 0xD800 && s->codeunits[i] <= 0xDBFF) {
            i += 1;
            charlen += 1;
        } else {
            charlen += 1;
        }
    }
    return charlen;
}

size_t jsstr16_get_utf16len(jsstr16_t *s) {
    return s->len;
}

size_t jsstr16_get_utf8len(jsstr16_t *s) {
    /* count along surrogate pairs, and use the UTF8_CLEN(c, l) macro from utf8.h to determine length */
    size_t utf8len = 0;
    for (size_t i = 0; i < s->len; i++) {
        wchar_t c;
        if (s->codeunits[i] >= 0xD800 && s->codeunits[i] <= 0xDBFF) {
            i += 1;
            c = 0x10000 + ((s->codeunits[i - 1] - 0xD800) << 10) + (s->codeunits[i] - 0xDC00);
        } else {
            c = s->codeunits[i];
        }
        int l;
        l = UTF8_CLEN(c);
        utf8len += l;
    }
    return utf8len;
}

uint16_t *jsstr16_get_at(jsstr16_t *s, size_t i) {
    /* i is code point index, so need to count surrogate pairs of two code units as one code point */
    /* need to walk from the beginning */
    size_t j;
    size_t k;
    for (j = 0, k = 0; j < s->len && k < i; j++, k++) {
        if (s->codeunits[j] >= 0xD800 && s->codeunits[j] <= 0xDBFF) {
            /* detected high surrogate */
            j++; /* skip the low surrogate */
        }
    }
    if (j < s->len) {
        return s->codeunits + j;
    } else {
        return NULL;
    }
}

void jsstr16_truncate(jsstr16_t *s, size_t len) {
    uint16_t *cu = jsstr16_get_at(s, len);
    if (cu != NULL) {
        s->len = cu - s->codeunits;
    }
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
    /* initialize from a slice of a source string, using jsstr8_get_at() to slice the buffer */
    s->bytes = jsstr8_get_at(src, start_i);
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

ssize_t jsstr8_indexof(jsstr8_t *s, wchar_t search_c, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* use UTF8_BLEN(bc, l) from utf8.h to determine byte length from character, and advance the cursor that amount */
    uint8_t *cc; /* character cursor */
    uint8_t *cs; /* character stop */
    wchar_t c; /* character */
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
ssize_t jsstr8_indextoken(jsstr8_t *s, wchar_t *search_c, size_t c_len, size_t start_i) {
    /* search for a code point in the string, and return the code point index (as works with _at() method) */
    /* use UTF8_BLEN(bc, l) from utf8.h to determine byte length from character, and advance the cursor that amount */
    uint8_t *cc; /* character cursor */
    uint8_t *cs; /* character stop */
    wchar_t c; /* character */
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

size_t jsstr8_set_from_utf8(jsstr8_t *s, uint8_t *str, size_t len) {
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

size_t jsstr8_get_charlen(jsstr8_t *s) {
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
    wchar_t c; /* character */
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

uint8_t *jsstr8_get_at(jsstr8_t *s, size_t i) {
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
    uint8_t *bc = jsstr8_get_at(s, len);
    if (bc != NULL) {
        s->len = bc - s->bytes;
    }
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
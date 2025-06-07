#ifndef JSSTR_H
#define JSSTR_H

/*
 * JavaScript string and character utilities for the whatwg spec
 * Implements:
 *  - string of code points (unicode characters)
 *  - string of code units (UTF-16)
 *  - string of bytes (UTF-8)
 */
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include <sys/types.h>

/*
 * JavaScript string of code points
 * A string of unicode characters.
 */
typedef struct jsstr_s {
    size_t cap;
    size_t len;
    wchar_t *codepoints;
} jsstr_t;

/*
 * JavaScript string of code units
 * A string of UTF-16 code units.
 */
typedef struct jsstr16_s {
    size_t cap;
    size_t len;
    uint16_t *codeunits;
} jsstr16_t;

/*
 * JavaScript byte sequence
 * A string of UTF-8 bytes.
 */
typedef struct jsstr8_s {
    size_t cap;
    size_t len;
    uint8_t *bytes;
} jsstr8_t;

size_t jsstr_head_size();

void jsstr8_init(jsstr8_t *s);
/*
 * Init a JavaScript string from a byte buffer.
 */
void jsstr_init_from_buf(jsstr_t *s, const char *buf, size_t len);
void jsstr_slice(jsstr_t *s, jsstr_t *src, size_t start_i, ssize_t stop_i);
int jsstr_cmp(jsstr_t *s1, jsstr_t *s2);
ssize_t jsstr_indexof(jsstr_t *s, wchar_t search_c, size_t start_i);
ssize_t jsstr_indextoken(jsstr_t *s, wchar_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr_get_cap(jsstr_t *s);

size_t jsstr_set_from_utf8(jsstr_t *s, uint8_t *str, size_t len);
#define jsstr_set_from_literal(s, str) jsstr_set_from_utf8(s, str, sizeof(str) - 1)
jsstr_t jsstr_from_str(const wchar_t *str);
#define declare_jsstr(__varname, __str) jsstr_t __varname = jsstr_from_str(__str)
static const jsstr_t jsstr_empty = {0, 0, NULL};

size_t jsstr_get_charlen(jsstr_t *s);
size_t jsstr_get_utf16len(jsstr_t *s);
size_t jsstr_get_utf8len(jsstr_t *s);

wchar_t *jsstr_get_at(jsstr_t *s, size_t i);
void jsstr_truncate(jsstr_t *s, size_t len);


void jsstr16_init(jsstr16_t *s);
/*
 * Init a JavaScript code unit string from a byte buffer.
 */
void jsstr16_init_from_buf(jsstr16_t *s, const char *buf, size_t len);
void jsstr16_slice(jsstr16_t *s, jsstr16_t *src, size_t start_i, ssize_t stop_i);
int jsstr16_cmp(jsstr16_t *s1, jsstr16_t *s2);
ssize_t jsstr16_indexof(jsstr16_t *s, wchar_t search_c, size_t start_i);
ssize_t jsstr16_indextoken(jsstr16_t *s, wchar_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr16_get_cap(jsstr16_t *s);

size_t jsstr16_set_from_utf8(jsstr16_t *s, uint8_t *str, size_t len);
#define jsstr16_set_from_literal(s, str) jsstr16_set_from_utf8(s, str, sizeof(str) - 1)
jsstr16_t jsstr16_from_str(const uint16_t *str);
#define declare_jsstr16(__varname, __str) jsstr16_t __varname = jsstr16_from_str(__str)
static const jsstr16_t jsstr16_empty = {0, 0, NULL};

size_t jsstr16_get_charlen(jsstr16_t *s);
size_t jsstr16_get_utf16len(jsstr16_t *s);
size_t jsstr16_get_utf8len(jsstr16_t *s);

uint16_t *jsstr16_get_at(jsstr16_t *s, size_t i);
void jsstr16_truncate(jsstr16_t *s, size_t len);


void jsstr8_init(jsstr8_t *s);
/*
 * Init a JavaScript utf-8 string from a byte buffer.
 */
void jsstr8_init_from_buf(jsstr8_t *s, const char *buf, size_t len);
void jsstr8_slice(jsstr8_t *s, jsstr8_t *src, size_t start_i, ssize_t stop_i);
int jsstr8_cmp(jsstr8_t *s1, jsstr8_t *s2);
ssize_t jsstr8_indexof(jsstr8_t *s, wchar_t search_c, size_t start_i);
ssize_t jsstr8_indextoken(jsstr8_t *s, wchar_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr8_get_cap(jsstr8_t *s);

size_t jsstr8_set_from_utf8(jsstr8_t *s, uint8_t *str, size_t len);
#define jsstr8_set_from_literal(s, str) jsstr8_set_from_utf8(s, str, sizeof(str) - 1)
jsstr8_t jsstr8_from_str(const char *str);
#define declare_jsstr8(__varname, __str) jsstr8_t __varname = jsstr8_from_str(__str)
static const jsstr8_t jsstr8_empty = {0, 0, NULL};

size_t jsstr8_get_charlen(jsstr8_t *s);
size_t jsstr8_get_utf16len(jsstr8_t *s);
size_t jsstr8_get_utf8len(jsstr8_t *s);

uint8_t *jsstr8_get_at(jsstr8_t *s, size_t i);
void jsstr8_truncate(jsstr8_t *s, size_t len);

#endif
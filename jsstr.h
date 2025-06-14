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
typedef struct jsstr32_s {
    size_t cap;
    size_t len;
    uint32_t *codepoints;
} jsstr32_t;

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

/*
 * uint32_t strings
 */

void jsstr8_init(jsstr8_t *s);
/*
 * Init a JavaScript string from a byte buffer.
 */
void jsstr32_init_from_buf(jsstr32_t *s, const char *buf, size_t len);
void jsstr32_slice(jsstr32_t *s, jsstr32_t *src, size_t start_i, ssize_t stop_i);
int jsstr32_cmp(jsstr32_t *s1, jsstr32_t *s2);
ssize_t jsstr32_indexof(jsstr32_t *s, uint32_t search_c, size_t start_i);
ssize_t jsstr32_indextoken(jsstr32_t *s, uint32_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr32_get_cap(jsstr32_t *s);

size_t jsstr32_set_from_utf32(jsstr32_t *s, const uint32_t *str, size_t len);
size_t jsstr32_set_from_utf16(jsstr32_t *s, const uint16_t *str, size_t len);
size_t jsstr32_set_from_utf8(jsstr32_t *s, uint8_t *str, size_t len);
#define jsstr32_set_from_literal(s, str) jsstr32_set_from_utf8(s, str, sizeof(str) - 1)
jsstr32_t jsstr32_from_str(const uint32_t *str);
#define declare_jsstr32(__varname, __str) jsstr_t __varname = jsstr32_from_str(__str)
static const jsstr32_t jsstr32_empty = {0, 0, NULL};

size_t jsstr32_get_utf32len(jsstr32_t *s);
size_t jsstr32_get_utf16len(jsstr32_t *s);
size_t jsstr32_get_utf8len(jsstr32_t *s);

uint32_t *jsstr32_wstr_codepoint_at(jsstr32_t *s, size_t i);
void jsstr32_truncate(jsstr32_t *s, size_t len);

/*
 * universal string methods
 */

/*
 * Return a string slicing just the single character at the index (forward or backward).
 * Returns an empty string if the index is out of bounds.
 * If the index is negative, it counts from the end of the string.
 * 
 */
jsstr32_t jsstr32_jsstr_codepoint_at2(jsstr32_t *s, ssize_t index);

/*
 * Return the character at the index as a string.
 */
jsstr32_t jsstr32_jsstr_codepoint_at(jsstr32_t *s, ssize_t index);

/*
 * Return the UTF-16 code unit at the index (in UTF-16 position).
 */
uint16_t jsstr32_u16_codeunit_at(jsstr32_t *s, ssize_t index, const char *buf, size_t len);

/*
 * Return the code point at the index (in code point position).
 */
uint32_t jsstr32_u32_codepoint_at(jsstr32_t *s, ssize_t index);

/*
 * Concatenate the src string to the end of the s string.
 * Returns 0 on success, or -1 (ENOBUFS) if there is not enough capacity.
 */
int jsstr32_concat(jsstr32_t *s, jsstr32_t *src);

/*
 * UTF-16 strings
 */

void jsstr16_init(jsstr16_t *s);
/*
 * Init a JavaScript code unit string from a byte buffer.
 */
void jsstr16_init_from_buf(jsstr16_t *s, const char *buf, size_t len);
void jsstr16_slice(jsstr16_t *s, jsstr16_t *src, size_t start_i, ssize_t stop_i);
int jsstr16_cmp(jsstr16_t *s1, jsstr16_t *s2);
ssize_t jsstr16_indexof(jsstr16_t *s, uint32_t search_c, size_t start_i);
ssize_t jsstr16_indextoken(jsstr16_t *s, uint32_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr16_get_cap(jsstr16_t *s);

size_t jsstr16_set_from_utf32(jsstr16_t *s, const uint32_t *str, size_t len);
size_t jsstr16_set_from_utf16(jsstr16_t *s, const uint16_t *str, size_t len);
size_t jsstr16_set_from_utf8(jsstr16_t *s, const uint8_t *str, size_t len);
#define jsstr16_set_from_literal(s, str) jsstr16_set_from_utf8(s, str, sizeof(str) - 1)
jsstr16_t jsstr16_from_str(const uint16_t *str);
#define declare_jsstr16(__varname, __str) jsstr16_t __varname = jsstr16_from_str(__str)
static const jsstr16_t jsstr16_empty = {0, 0, NULL};

size_t jsstr16_get_utf32len(jsstr16_t *s);
size_t jsstr16_get_utf16len(jsstr16_t *s);
size_t jsstr16_get_utf8len(jsstr16_t *s);

uint16_t *jsstr16_u16s_codeunit_at(jsstr16_t *s, size_t i);
void jsstr16_truncate(jsstr16_t *s, size_t len);

jsstr16_t jsstr16_jsstr16_codepoint_at2(jsstr16_t *s, ssize_t index);
jsstr16_t jsstr16_jsstr16_codepoint_at(jsstr16_t *s, ssize_t index);
uint16_t jsstr16_u16_codeunit_at(jsstr16_t *s, ssize_t index);
uint32_t jsstr16_u32_codepoint_at(jsstr16_t *s, ssize_t index, const char *buf, size_t len);
int jsstr16_concat(jsstr16_t *s, jsstr16_t *src);

/*
 * UTF-8 strings
 */

void jsstr8_init(jsstr8_t *s);
/*
 * Init a JavaScript utf-8 string from a byte buffer.
 */
void jsstr8_init_from_buf(jsstr8_t *s, const char *buf, size_t len);
void jsstr8_slice(jsstr8_t *s, jsstr8_t *src, size_t start_i, ssize_t stop_i);
int jsstr8_cmp(jsstr8_t *s1, jsstr8_t *s2);
ssize_t jsstr8_indexof(jsstr8_t *s, uint32_t search_c, size_t start_i);
ssize_t jsstr8_indextoken(jsstr8_t *s, uint32_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr8_get_cap(jsstr8_t *s);

size_t jsstr8_set_from_utf32(jsstr8_t *s, const uint32_t *str, size_t len);
size_t jsstr8_set_from_utf16(jsstr8_t *s, const uint16_t *str, size_t len);
size_t jsstr8_set_from_utf8(jsstr8_t *s, const uint8_t *str, size_t len);
#define jsstr8_set_from_literal(s, str) jsstr8_set_from_utf8(s, str, sizeof(str) - 1)
jsstr8_t jsstr8_from_str(const char *str);
#define declare_jsstr8(__varname, __str) jsstr8_t __varname = jsstr8_from_str(__str)
static const jsstr8_t jsstr8_empty = {0, 0, NULL};

size_t jsstr8_get_utf32len(jsstr8_t *s);
size_t jsstr8_get_utf16len(jsstr8_t *s);
size_t jsstr8_get_utf8len(jsstr8_t *s);

uint8_t *jsstr8_u8s_byte_at(jsstr8_t *s, size_t i);
void jsstr8_truncate(jsstr8_t *s, size_t len);

jsstr8_t jsstr8_jsstr8_codepoint_at2(jsstr8_t *s, ssize_t index);
jsstr8_t jsstr8_jsstr8_codepoint_at(jsstr8_t *s, ssize_t index);
uint16_t jsstr8_u16_codeunit_at(jsstr8_t *s, ssize_t index, const char *buf, size_t len);
uint32_t jsstr8_u32_codepoint_at(jsstr8_t *s, ssize_t index, const char *buf, size_t len);
int jsstr8_concat(jsstr8_t *s, jsstr8_t *src);

#endif
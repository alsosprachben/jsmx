#ifndef JSSTR_H
#define JSSTR_H

/*
 * String and encoding utilities for high-level string semantics.
 * Implements:
 *  - string of code points (UTF-32)
 *  - string of code units (UTF-16)
 *  - string of bytes (UTF-8)
 * 
 * The stings are slice pointers:
 *  - capacity,
 *  - length, and
 *  - a pointer to the data.
 * 
 * They do not contain the data themselves, 
 *   but rather point to a buffer that is managed elsewhere.
 * 
 * Therefore, these are *mutable* strings.
 * 
 * These are NOT null-terminated strings:
 *  - There may be other data next to the string data,
 *  - so *never* assume null termination.
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

size_t jsstr32_head_size();
size_t jsstr16_head_size();
size_t jsstr8_head_size();

/* element length, not character length */
size_t utf8_strlen(const uint8_t *str);
size_t utf16_strlen(const uint16_t *str);
size_t utf32_strlen(const uint32_t *str);

/*
 * uint32_t strings
 */

void jsstr8_init(jsstr8_t *s);
/*
 * Init a JavaScript string from a byte buffer.
 * This initializes an empty string with a given capacity.
 */
void jsstr32_init_from_buf(jsstr32_t *s, const char *buf, size_t len);
/*
 * Set the string from a null-terminated wide character string.
 * The number of code points processed is determined by the length of the string.
 */
void jsstr32_init_from_str(jsstr32_t *s, const uint32_t *str);
#define declare_jsstr32(__varname, __str) \
    jsstr_t __varname; \
    jsstr32_init_from_str(&__varname, __str)
static const jsstr32_t jsstr32_empty = {0, 0, NULL};

void jsstr32_u32_slice(jsstr32_t *s, jsstr32_t *src, size_t start_i, ssize_t stop_i);
int jsstr32_u32_cmp(jsstr32_t *s1, jsstr32_t *s2);

/*
 * Find the index of the first occurrence of the code point in the string.
 * Returns the index of the code point, or -1 if not found.
 */
ssize_t jsstr32_u32_indexof(jsstr32_t *s, uint32_t search_c, size_t start_i);
/*
 * Find the index of the first occurrence of a sequence of code points in the string.
 * Returns the index of the first code point of the sequence, or -1 if not found.
 */
ssize_t jsstr32_u32_indextoken(jsstr32_t *s, uint32_t *search_c, size_t search_c_len, size_t start_i);

/* return the fixed capacity of the string */
size_t jsstr32_get_cap(jsstr32_t *s);

/*
 * Set the string from a UTF-32, UTF-16 or UTF-8 encoded string.
 * Returns the number of code points processed.
 * Therefore, you can use a cursor to iterate a single string over a single buffer
 * by incrementing the cursor by the return value
 * until it reaches the end of the buffer.
 */
size_t jsstr32_set_from_utf32(jsstr32_t *s, const uint32_t *str, size_t len);
size_t jsstr32_set_from_utf16(jsstr32_t *s, const uint16_t *str, size_t len);
size_t jsstr32_set_from_utf8(jsstr32_t *s, uint8_t *str, size_t len);
#define jsstr32_set_from_literal(s, str) jsstr32_set_from_utf8(s, str, sizeof(str) - 1)

size_t jssstr32_set_from_jsstr16(jsstr32_t *s, jsstr16_t *src);
size_t jssstr32_set_from_jsstr8(jsstr32_t *s, jsstr8_t *src);

size_t jsstr32_get_utf32len(jsstr32_t *s);
size_t jsstr32_get_utf16len(jsstr32_t *s);
size_t jsstr32_get_utf8len(jsstr32_t *s);

uint32_t *jsstr32_u32s_at(jsstr32_t *s, size_t i);
void jsstr32_u32_truncate(jsstr32_t *s, size_t len);

/*
 * universal string methods
 */

/*
 * Return the character at the index as a string.
 */
jsstr32_t jsstr32_jsstr32_at(jsstr32_t *s, ssize_t index);

/*
 * Return the UTF-16 code unit at the index (in UTF-16 position).
 */
uint16_t jsstr32_u16_at(jsstr32_t *s, ssize_t index, const char *buf, size_t len);

/*
 * Return the code point at the index (in code point position).
 */
uint32_t jsstr32_u32_at(jsstr32_t *s, ssize_t index);

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
void jsstr16_init_from_str(jsstr16_t *s, const uint16_t *str);
#define declare_jsstr16(__varname, __str) \
    jsstr16_t __varname; \
    jsstr16_init_from_str(&__varname, __str)
static const jsstr16_t jsstr16_empty = {0, 0, NULL};

void jsstr16_u16_slice(jsstr16_t *s, jsstr16_t *src, size_t start_i, ssize_t stop_i);
int jsstr16_u16_cmp(jsstr16_t *s1, jsstr16_t *s2);
ssize_t jsstr16_u16_indexof(jsstr16_t *s, uint16_t search_c, size_t start_i);
ssize_t jsstr16_u32_indexof(jsstr16_t *s, uint32_t search_c, size_t start_i);
ssize_t jsstr16_u16_indextoken(jsstr16_t *s, uint16_t *search_c, size_t search_c_len, size_t start_i);
ssize_t jsstr16_u32_indextoken(jsstr16_t *s, uint32_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr16_get_cap(jsstr16_t *s);

size_t jsstr16_set_from_utf32(jsstr16_t *s, const uint32_t *str, size_t len);
size_t jsstr16_set_from_utf16(jsstr16_t *s, const uint16_t *str, size_t len);
size_t jsstr16_set_from_utf8(jsstr16_t *s, const uint8_t *str, size_t len);
#define jsstr16_set_from_literal(s, str) jsstr16_set_from_utf8(s, str, sizeof(str) - 1)

size_t jssstr16_set_from_jsstr32(jsstr16_t *s, jsstr32_t *src);
size_t jssstr16_set_from_jsstr8(jsstr16_t *s, jsstr8_t *src);

size_t jsstr16_get_utf32len(jsstr16_t *s);
size_t jsstr16_get_utf16len(jsstr16_t *s);
size_t jsstr16_get_utf8len(jsstr16_t *s);

uint16_t *jsstr16_u16s_at(jsstr16_t *s, size_t i);
void jsstr16_u16_truncate(jsstr16_t *s, size_t len);

jsstr16_t jsstr16_jsstr16_at(jsstr16_t *s, ssize_t index);
uint16_t jsstr16_u16_at(jsstr16_t *s, ssize_t index);
uint32_t jsstr16_u32_at(jsstr16_t *s, ssize_t index, const char *buf, size_t len);
int jsstr16_concat(jsstr16_t *s, jsstr16_t *src);

/*
 * UTF-8 strings
 */

void jsstr8_init(jsstr8_t *s);
/*
 * Init a JavaScript utf-8 string from a byte buffer.
 */
void jsstr8_init_from_buf(jsstr8_t *s, const char *buf, size_t len);
void jsstr8_init_from_str(jsstr8_t *s, const char *str);
#define declare_jsstr8(__varname, __str) \
    jsstr8_t __varname; \
    jsstr8_init_from_str(&__varname, __str)
static const jsstr8_t jsstr8_empty = {0, 0, NULL};

void jsstr8_u8_slice(jsstr8_t *s, jsstr8_t *src, size_t start_i, ssize_t stop_i);
int jsstr8_u8_cmp(jsstr8_t *s1, jsstr8_t *s2);
ssize_t jsstr8_u8_indexof(jsstr8_t *s, uint8_t search_c, size_t start_i);
ssize_t jsstr8_u32_indexof(jsstr8_t *s, uint32_t search_c, size_t start_i);
ssize_t jsstr8_u8_indextoken(jsstr8_t *s, uint8_t *search_c, size_t search_c_len, size_t start_i);
ssize_t jsstr8_u32_indextoken(jsstr8_t *s, uint32_t *search_c, size_t search_c_len, size_t start_i);
size_t jsstr8_get_cap(jsstr8_t *s);

size_t jsstr8_set_from_utf32(jsstr8_t *s, const uint32_t *str, size_t len);
size_t jsstr8_set_from_utf16(jsstr8_t *s, const uint16_t *str, size_t len);
size_t jsstr8_set_from_utf8(jsstr8_t *s, const uint8_t *str, size_t len);
#define jsstr8_set_from_literal(s, str) jsstr8_set_from_utf8(s, str, sizeof(str) - 1)

size_t jssstr8_set_from_jsstr32(jsstr8_t *s, jsstr32_t *src);
size_t jssstr8_set_from_jsstr16(jsstr8_t *s, jsstr16_t *src);

size_t jsstr8_get_utf32len(jsstr8_t *s);
size_t jsstr8_get_utf16len(jsstr8_t *s);
size_t jsstr8_get_utf8len(jsstr8_t *s);

uint8_t *jsstr8_u8s_at(jsstr8_t *s, size_t i);
void jsstr8_u8_truncate(jsstr8_t *s, size_t len);

jsstr8_t jsstr8_jsstr8_at(jsstr8_t *s, ssize_t index);
uint16_t jsstr8_u16_at(jsstr8_t *s, ssize_t index, const char *buf, size_t len);
uint32_t jsstr8_u32_at(jsstr8_t *s, ssize_t index, const char *buf, size_t len);
int jsstr8_concat(jsstr8_t *s, jsstr8_t *src);

#endif
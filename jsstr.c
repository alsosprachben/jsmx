#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include "jsstr.h"
#include "utf8.h"
#include "unicode.h"

/*
 * Locale helpers
 *
 * The build system can define JS_USE_LIBC_LOCALE or JS_USE_ICU_LOCALE to
 * enable locale-aware conversions using libc or ICU respectively.  If none are
 * defined, a small ASCII-only fallback is used instead.
 */

#ifndef JS_USE_ICU_LOCALE
#define JS_USE_ICU_LOCALE 0
#endif
#ifndef JS_USE_LIBC_LOCALE
#define JS_USE_LIBC_LOCALE 0
#endif

#if JS_USE_ICU_LOCALE
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/ucol.h>
#include <unicode/ustring.h>
#define JS_LOCALE_TO_LOWER(cp) ((uint32_t)u_tolower(cp))
#define JS_LOCALE_TO_UPPER(cp) ((uint32_t)u_toupper(cp))
#define JS_LOCALE_TO_LOWER_FULL(cp,out) ((out)[0]=(uint32_t)u_tolower(cp),1)
#define JS_LOCALE_TO_UPPER_FULL(cp,out) ((out)[0]=(uint32_t)u_toupper(cp),1)
#define JS_LOCALE_TO_LOWER_FULL_L(cp,locale,out) \
    unicode_tolower_full_locale(cp, locale, out)
#define JS_LOCALE_TO_UPPER_FULL_L(cp,locale,out) \
    unicode_toupper_full_locale(cp, locale, out)
#define JS_LOCALE_IS_SPACE(cp) (u_isUWhiteSpace(cp))
static inline uint32_t js_locale_icu_normalize_char(uint32_t cp) {
    UErrorCode status = U_ZERO_ERROR;
    const UNorm2 *norm = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status)) return cp;
    UChar src[4];
    int32_t len = 0;
    U16_APPEND_UNSAFE(src, len, cp);
    UChar dest[4];
    int32_t dlen = unorm2_normalize(norm, src, len, dest, 4, &status);
    if (U_FAILURE(status) || dlen <= 0) return cp;
    UChar32 out;
    int32_t i = 0;
    U16_NEXT_UNSAFE(dest, i, out);
    return (uint32_t)out;
}

static inline int js_locale_icu_compare_char(uint32_t a, uint32_t b) {
    UChar sa[4];
    UChar sb[4];
    int32_t la = 0;
    int32_t lb = 0;
    U16_APPEND_UNSAFE(sa, la, a);
    U16_APPEND_UNSAFE(sb, lb, b);
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open("", &status);
    if (U_FAILURE(status)) {
        return (int)(a - b);
    }
    int result = ucol_strcoll(coll, sa, la, sb, lb);
    ucol_close(coll);
    return result;
}
#define JS_LOCALE_NORMALIZE(cp) js_locale_icu_normalize_char(cp)
#define JS_LOCALE_COMPARE(a,b) js_locale_icu_compare_char(a,b)

static inline int32_t js_locale_icu_normalize_u16(UChar *buf, int32_t len, int32_t cap) {
    UErrorCode status = U_ZERO_ERROR;
    const UNorm2 *norm = unorm2_getNFCInstance(&status);
    if (U_FAILURE(status)) return len;
    UChar *tmp = (UChar *)malloc(sizeof(UChar) * cap);
    if (!tmp) return len;
    int32_t out = unorm2_normalize(norm, buf, len, tmp, cap, &status);
    if (U_FAILURE(status)) {
        free(tmp);
        return len;
    }
    memcpy(buf, tmp, out * sizeof(UChar));
    free(tmp);
    return out;
}

static inline int js_locale_icu_compare_u16(const UChar *a, int32_t la, const UChar *b, int32_t lb) {
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open("", &status);
    if (U_FAILURE(status)) return 0;
    int r = ucol_strcoll(coll, a, la, b, lb);
    ucol_close(coll);
    return r;
}

static inline int32_t js_locale_icu_normalize_u32(uint32_t *buf, int32_t len, int32_t cap) {
    UChar *tmp = (UChar *)malloc(sizeof(UChar) * cap * 2);
    if (!tmp)
        return len;
    int32_t off = 0;
    for (int32_t i = 0; i < len; i++) {
        U16_APPEND_UNSAFE(tmp, off, buf[i]);
    }
    int32_t out_len = js_locale_icu_normalize_u16(tmp, off, cap * 2);
    int32_t pos = 0;
    int32_t out_cp = 0;
    while (pos < out_len && out_cp < cap) {
        UChar32 c;
        U16_NEXT_UNSAFE(tmp, pos, c);
        buf[out_cp++] = (uint32_t)c;
    }
    free(tmp);
    return out_cp;
}

static inline int js_locale_icu_compare_u32(const uint32_t *a, int32_t la, const uint32_t *b, int32_t lb) {
    UChar *ua = (UChar *)malloc(sizeof(UChar) * la * 2);
    UChar *ub = (UChar *)malloc(sizeof(UChar) * lb * 2);
    if (!ua || !ub) {
        free(ua);
        free(ub);
        return 0;
    }
    int32_t la16 = 0, lb16 = 0;
    for (int32_t i = 0; i < la; i++)
        U16_APPEND_UNSAFE(ua, la16, a[i]);
    for (int32_t i = 0; i < lb; i++)
        U16_APPEND_UNSAFE(ub, lb16, b[i]);
    int r = js_locale_icu_compare_u16(ua, la16, ub, lb16);
    free(ua);
    free(ub);
    return r;
}

static inline int32_t js_locale_icu_normalize_u8(uint8_t *buf, int32_t len, int32_t cap) {
    UChar *tmp = (UChar *)malloc(sizeof(UChar) * cap * 2);
    if (!tmp)
        return len;
    int32_t u16len = 0;
    for (int32_t i = 0; i < len;) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *)buf + i, (const char *)buf + len, &c, &l);
        U16_APPEND_UNSAFE(tmp, u16len, c);
        i += l;
    }
    int32_t out_len = js_locale_icu_normalize_u16(tmp, u16len, cap * 2);
    uint8_t *write = buf;
    uint8_t *end = buf + cap;
    int32_t pos = 0;
    while (pos < out_len && write < end) {
        UChar32 c;
        U16_NEXT_UNSAFE(tmp, pos, c);
        const uint32_t *cc = (const uint32_t *)&c;
        UTF8_ENCODE(&cc, &c + 1, &write, end);
    }
    free(tmp);
    return (int32_t)(write - buf);
}

static inline int js_locale_icu_compare_u8(const uint8_t *a, int32_t la, const uint8_t *b, int32_t lb) {
    UChar *ua = (UChar *)malloc(sizeof(UChar) * la * 2);
    UChar *ub = (UChar *)malloc(sizeof(UChar) * lb * 2);
    if (!ua || !ub) {
        free(ua);
        free(ub);
        return 0;
    }
    int32_t la16 = 0, lb16 = 0;
    for (int32_t i = 0; i < la;) {
        uint32_t c; int l; UTF8_CHAR((const char *)a + i, (const char *)a + la, &c, &l); i += l; U16_APPEND_UNSAFE(ua, la16, c);
    }
    for (int32_t i = 0; i < lb;) {
        uint32_t c; int l; UTF8_CHAR((const char *)b + i, (const char *)b + lb, &c, &l); i += l; U16_APPEND_UNSAFE(ub, lb16, c);
    }
    int r = js_locale_icu_compare_u16(ua, la16, ub, lb16);
    free(ua);
    free(ub);
    return r;
}
#define JS_LOCALE_NORMALIZE_U32 js_locale_icu_normalize_u32
#define JS_LOCALE_COMPARE_U32  js_locale_icu_compare_u32
#define JS_LOCALE_NORMALIZE_U16 js_locale_icu_normalize_u16
#define JS_LOCALE_COMPARE_U16  js_locale_icu_compare_u16
#define JS_LOCALE_NORMALIZE_U8  js_locale_icu_normalize_u8
#define JS_LOCALE_COMPARE_U8    js_locale_icu_compare_u8
#elif JS_USE_LIBC_LOCALE
#include <wctype.h>
#include <ctype.h>
#include <wchar.h>
#define JS_LOCALE_TO_LOWER(cp) ((uint32_t)towlower((wint_t)(cp)))
#define JS_LOCALE_TO_UPPER(cp) ((uint32_t)towupper((wint_t)(cp)))
#define JS_LOCALE_TO_LOWER_FULL(cp,out) ((out)[0]=(uint32_t)towlower((wint_t)(cp)),1)
#define JS_LOCALE_TO_UPPER_FULL(cp,out) ((out)[0]=(uint32_t)towupper((wint_t)(cp)),1)
#define JS_LOCALE_TO_LOWER_FULL_L(cp,locale,out) \
    unicode_tolower_full_locale(cp, locale, out)
#define JS_LOCALE_TO_UPPER_FULL_L(cp,locale,out) \
    unicode_toupper_full_locale(cp, locale, out)
#define JS_LOCALE_IS_SPACE(cp) (iswspace((wint_t)(cp)))
static inline uint32_t js_locale_libc_normalize(uint32_t cp) {
    return cp; /* no normalization API in libc */
}
static inline int js_locale_libc_compare(uint32_t a, uint32_t b) {
    wchar_t sa[2] = {(wchar_t)a, L'\0'};
    wchar_t sb[2] = {(wchar_t)b, L'\0'};
    return wcscoll(sa, sb);
}
#define JS_LOCALE_NORMALIZE(cp) js_locale_libc_normalize(cp)
#define JS_LOCALE_COMPARE(a,b) js_locale_libc_compare(a,b)

static inline int32_t js_locale_libc_normalize_u32(uint32_t *buf, int32_t len, int32_t cap) {
    (void)buf; (void)cap; return len;
}
static inline int32_t js_locale_libc_normalize_u16(uint16_t *buf, int32_t len, int32_t cap) {
    (void)buf; (void)cap; return len;
}
static inline int32_t js_locale_libc_normalize_u8(uint8_t *buf, int32_t len, int32_t cap) {
    (void)buf; (void)cap; return len;
}

static inline int js_locale_libc_compare_u16(const uint16_t *a, int32_t la, const uint16_t *b, int32_t lb) {
    wchar_t *wa = (wchar_t *)malloc(sizeof(wchar_t) * (la + 1));
    wchar_t *wb = (wchar_t *)malloc(sizeof(wchar_t) * (lb + 1));
    if (!wa || !wb) { free(wa); free(wb); return 0; }
    for (int32_t i = 0; i < la; i++) wa[i] = (wchar_t)a[i];
    wa[la] = L'\0';
    for (int32_t i = 0; i < lb; i++) wb[i] = (wchar_t)b[i];
    wb[lb] = L'\0';
    int r = wcscoll(wa, wb);
    free(wa); free(wb);
    return r;
}

static inline int js_locale_libc_compare_u32(const uint32_t *a, int32_t la, const uint32_t *b, int32_t lb) {
    return js_locale_libc_compare_u16((const uint16_t *)a, la, (const uint16_t *)b, lb);
}

static inline int js_locale_libc_compare_u8(const uint8_t *a, int32_t la, const uint8_t *b, int32_t lb) {
    return js_locale_libc_compare_u16((const uint16_t *)a, la, (const uint16_t *)b, lb);
}

#define JS_LOCALE_NORMALIZE_U32 js_locale_libc_normalize_u32
#define JS_LOCALE_COMPARE_U32  js_locale_libc_compare_u32
#define JS_LOCALE_NORMALIZE_U16 js_locale_libc_normalize_u16
#define JS_LOCALE_COMPARE_U16  js_locale_libc_compare_u16
#define JS_LOCALE_NORMALIZE_U8  js_locale_libc_normalize_u8
#define JS_LOCALE_COMPARE_U8    js_locale_libc_compare_u8
#else
#include "unicode.h"

static inline uint32_t js_locale_stub_to_lower(uint32_t cp) {
    return unicode_tolower(cp);
}
static inline uint32_t js_locale_stub_to_upper(uint32_t cp) {
    return unicode_toupper(cp);
}
static inline size_t js_locale_stub_to_lower_full(uint32_t cp, uint32_t out[3]) {
    return unicode_tolower_full(cp, out);
}
static inline size_t js_locale_stub_to_upper_full(uint32_t cp, uint32_t out[3]) {
    return unicode_toupper_full(cp, out);
}
static inline int js_locale_stub_is_space(uint32_t cp) {
    switch (cp) {
    case ' ': case '\t': case '\n': case '\r': case '\f': case '\v':
        return 1;
    default:
        return 0;
    }
}
#define JS_LOCALE_TO_LOWER(cp) js_locale_stub_to_lower(cp)
#define JS_LOCALE_TO_UPPER(cp) js_locale_stub_to_upper(cp)
#define JS_LOCALE_TO_LOWER_FULL(cp,out) js_locale_stub_to_lower_full(cp,out)
#define JS_LOCALE_TO_UPPER_FULL(cp,out) js_locale_stub_to_upper_full(cp,out)
#define JS_LOCALE_TO_LOWER_FULL_L(cp,locale,out) \
    unicode_tolower_full_locale(cp, locale, out)
#define JS_LOCALE_TO_UPPER_FULL_L(cp,locale,out) \
    unicode_toupper_full_locale(cp, locale, out)
#define JS_LOCALE_IS_SPACE(cp) js_locale_stub_is_space(cp)
#define JS_LOCALE_NORMALIZE(cp) (cp)

static inline int js_locale_stub_compare_char(uint32_t a, uint32_t b) {
    uint16_t pa = 0, sa = 0, ta = 0;
    uint16_t pb = 0, sb = 0, tb = 0;
    unicode_collation_lookup(a, &pa, &sa, &ta);
    unicode_collation_lookup(b, &pb, &sb, &tb);
    if (pa != pb) return (int)pa - (int)pb;
    if (sa != sb) return (int)sa - (int)sb;
    if (ta != tb) return (int)ta - (int)tb;
    return (int)a - (int)b;
}

#define JS_LOCALE_COMPARE(a,b) js_locale_stub_compare_char(a,b)

static inline int32_t js_locale_stub_normalize_u32(uint32_t *buf, int32_t len, int32_t cap) {
    (void)cap;
    for (int32_t i = 0; i < len; i++) buf[i] = JS_LOCALE_NORMALIZE(buf[i]);
    return len;
}
static inline int32_t js_locale_stub_normalize_u16(uint16_t *buf, int32_t len, int32_t cap) {
    (void)cap; for (int32_t i=0;i<len;i++) buf[i]=JS_LOCALE_NORMALIZE(buf[i]); return len;
}
static inline int32_t js_locale_stub_normalize_u8(uint8_t *buf, int32_t len, int32_t cap) {
    (void)cap; for (int32_t i=0;i<len;i++) buf[i]=JS_LOCALE_NORMALIZE(buf[i]); return len;
}

static inline int js_locale_stub_compare_u32(const uint32_t *a, int32_t la, const uint32_t *b, int32_t lb) {
    int32_t i = 0;
    int32_t j = 0;
    while (i < la && j < lb) {
        int r = js_locale_stub_compare_char(a[i], b[j]);
        if (r != 0)
            return r;
        i++; j++;
    }
    return la - lb;
}
static inline int js_locale_stub_compare_u16(const uint16_t *a, int32_t la, const uint16_t *b, int32_t lb) {
    int32_t ia = 0;
    int32_t ib = 0;
    while (ia < la && ib < lb) {
        uint32_t ca, cb;
        int la1, lb1;
        UTF16_CHAR(a + ia, a + la, &ca, &la1);
        UTF16_CHAR(b + ib, b + lb, &cb, &lb1);
        if (la1 <= 0) { la1 = la1 ? -la1 : 1; ca = 0xFFFD; }
        if (lb1 <= 0) { lb1 = lb1 ? -lb1 : 1; cb = 0xFFFD; }
        ia += la1; ib += lb1;
        int r = js_locale_stub_compare_char(ca, cb);
        if (r != 0)
            return r;
    }
    return la - lb;
}
static inline int js_locale_stub_compare_u8(const uint8_t *a, int32_t la, const uint8_t *b, int32_t lb) {
    int32_t ia = 0;
    int32_t ib = 0;
    while (ia < la && ib < lb) {
        uint32_t ca, cb;
        int la1, lb1;
        UTF8_CHAR(a + ia, a + la, &ca, &la1);
        UTF8_CHAR(b + ib, b + lb, &cb, &lb1);
        if (la1 <= 0) { la1 = la1 ? -la1 : 1; ca = 0xFFFD; }
        if (lb1 <= 0) { lb1 = lb1 ? -lb1 : 1; cb = 0xFFFD; }
        ia += la1; ib += lb1;
        int r = js_locale_stub_compare_char(ca, cb);
        if (r != 0)
            return r;
    }
    return la - lb;
}

#define JS_LOCALE_NORMALIZE_U32 js_locale_stub_normalize_u32
#define JS_LOCALE_COMPARE_U32  js_locale_stub_compare_u32
#define JS_LOCALE_NORMALIZE_U16 js_locale_stub_normalize_u16
#define JS_LOCALE_COMPARE_U16  js_locale_stub_compare_u16
#define JS_LOCALE_NORMALIZE_U8  js_locale_stub_normalize_u8
#define JS_LOCALE_COMPARE_U8    js_locale_stub_compare_u8
#endif

static int jsstr16_requires_full_case(jsstr16_t *s, int upper)
{
    uint16_t *read = s->codeunits;
    uint16_t *end = s->codeunits + s->len;
    while (read < end) {
        uint32_t c;
        int l;
        UTF16_CHAR(read, end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        read += l;
        uint32_t tmp[3];
        size_t n = upper ? unicode_toupper_full(c, tmp) : unicode_tolower_full(c, tmp);
        if (n != 1)
            return 1;
    }
    return 0;
}

static int jsstr8_requires_full_case(jsstr8_t *s, int upper)
{
    uint8_t *read = s->bytes;
    uint8_t *end = s->bytes + s->len;
    while (read < end) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *)read, (const char *)end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        read += l;
        uint32_t tmp[3];
        size_t n = upper ? unicode_toupper_full(c, tmp) : unicode_tolower_full(c, tmp);
        if (n != 1)
            return 1;
    }
    return 0;
}



size_t jsstr32_head_size() {
    return sizeof(jsstr32_t);
}
size_t jsstr16_head_size() {
    return sizeof(jsstr16_t);
}
size_t jsstr8_head_size() {
    return sizeof(jsstr8_t);
}

size_t utf8_strlen(const uint8_t *str) {
    /* element length, not characterlength */
    size_t len = 0;
    const uint8_t *p = str;
    for (; *p; p++) {
        len++;
    }
    return len;
}
size_t utf16_strlen(const uint16_t *str) {
    /* element length, not character length */
    size_t len = 0;
    const uint16_t *p = str;
    for (; *p; p++) {
        len++;
    }
    return len;
}
size_t utf32_strlen(const uint32_t *str) {
    /* element length, not character length */
    size_t len = 0;
    const uint32_t *p = str;
    for (; *p; p++) {
        len++;
    }
    return len;
}

void jsstr32_init(jsstr32_t *s) {
    s->cap = 0;
    s->len = 0;
    s->codepoints = NULL;
}

void jsstr32_init_from_buf(jsstr32_t *s, const char *buf, size_t len) {
    s->cap = len / sizeof(uint32_t);
    s->len = 0;
    s->codepoints = (uint32_t *) buf;
}

void jsstr32_init_from_str(jsstr32_t *s, const uint32_t *str) {
    size_t len = utf32_strlen(str);
    jsstr32_init_from_buf(s, (char *) str, len);
    s->len = len;
    return;
}

void jsstr32_u32_slice(jsstr32_t *s, jsstr32_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr32_u32s_at() to slice the buffer */
    s->codepoints = jsstr32_u32s_at(src, start_i);
    s->len = src->codepoints + src->len - s->codepoints;
    if (stop_i >= 0) {
        jsstr32_u32_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr32_u32_cmp(jsstr32_t *s1, jsstr32_t *s2) {
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

ssize_t jsstr32_u32_indexof(jsstr32_t *s, uint32_t c, size_t start_i) {
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && s->codepoints[i] == c) {
            return i;
        }
    }
    return -i;
}

ssize_t jsstr32_u32_indextoken(jsstr32_t *s, uint32_t *c, size_t c_len, size_t start_i) {
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
    size_t to_send = len <= s->cap ? len : s->cap;
    memcpy(s->codepoints, str, to_send * sizeof(uint32_t));
    s->len = len;
    return to_send; /* return the number of code points processed */
}

size_t jsstr32_set_from_utf16(jsstr32_t *s, const uint16_t *str, size_t len) {
    /* decode each UTF-16 code unit into a code point elements, assigning into codepoints array */
    const uint16_t *bc = str;
    const uint16_t *bs = str + len;
    uint32_t *cc = s->codepoints;
    uint32_t *cs = s->codepoints + s->cap;
    UTF16_DECODE((const uint16_t **) &bc, (const uint16_t *) bs, &cc, cs);
    s->len = cc - s->codepoints;
    return bc - str; /* return the number of code units processed */
}

size_t jsstr32_set_from_utf8(jsstr32_t *s, uint8_t *str, size_t len) {
    /* decode each utf-8 sequence into a code point elements, assigning into codepoints array */
    uint8_t *bc = str;
    uint8_t *bs = str + len;
    uint32_t *cc = s->codepoints;
    uint32_t *cs = s->codepoints + s->cap;
    UTF8_DECODE((const uint8_t **) &bc, (const uint8_t *) bs, &cc, cs);
    s->len = cc - s->codepoints;
    return bc - str; /* return the number of bytes processed */
}

size_t jsstr32_set_from_jsstr16(jsstr32_t *s, jsstr16_t *src) {
    /* convert a jsstr16_t string to a jsstr32_t string */
    return jsstr32_set_from_utf16(s, src->codeunits, src->len);
}

size_t jsstr32_set_from_jsstr8(jsstr32_t *s, jsstr8_t *src) {
    /* convert a jsstr8_t string to a jsstr32_t string */
    return jsstr32_set_from_utf8(s, src->bytes, src->len);
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

int jsstr32_is_well_formed(jsstr32_t *s) {
    /* check if the string is well-formed UTF-32 */
    return UTF32_WELL_FORMED(s->codepoints, s->codepoints + s->len);
}

void jsstr32_to_well_formed(jsstr32_t *s) {
    /* convert the string to well-formed UTF-32 by replacing invalid code points with U+FFFD */
    UTF32_TO_WELL_FORMED(s->codepoints, s->codepoints + s->len);
}

uint32_t *jsstr32_u32s_at(jsstr32_t *s, size_t i) {
    if (i < s->len) {
        return s->codepoints + i;
    } else {
        return NULL;
    }
}

void jsstr32_u32_truncate(jsstr32_t *s, size_t len) {
    uint32_t *c = jsstr32_u32s_at(s, len);
    if (c != NULL) {
        s->len = c - s->codepoints;
    }
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
static int js_is_cased(uint32_t cp) {
    return unicode_tolower(cp) != unicode_toupper(cp);
}
static int js_is_final_sigma32(const uint32_t *buf, size_t len, size_t pos) {
    if (buf[pos] != 0x03A3) return 0;
    size_t j = pos;
    int has_prev = 0;
    while (j > 0) {
        --j;
        if (js_is_cased(buf[j])) { has_prev = 1; break; }
    }
    if (!has_prev) return 0;
    for (j = pos + 1; j < len; j++) {
        if (js_is_cased(buf[j])) return 0;
    }
    return 1;
}


void jsstr32_tolower(jsstr32_t *s) {
    size_t i = 0;
    while (i < s->len) {
        uint32_t seq[3];
        size_t n;
        if (s->codepoints[i] == 0x03A3 &&
            js_is_final_sigma32(s->codepoints, s->len, i)) {
            seq[0] = 0x03C2;
            n = 1;
        } else {
            n = JS_LOCALE_TO_LOWER_FULL(s->codepoints[i], seq);
        }
        if (n == 1) {
            s->codepoints[i] = seq[0];
            i++;
        } else {
            if (s->len + (n - 1) > s->cap)
                break;
            memmove(s->codepoints + i + n,
                    s->codepoints + i + 1,
                    (s->len - i - 1) * sizeof(uint32_t));
            for (size_t j = 0; j < n; j++)
                s->codepoints[i + j] = seq[j];
            s->len += n - 1;
            i += n;
        }
    }
}

void jsstr32_tolower_locale(jsstr32_t *s, const char *locale) {
    size_t i = 0;
    while (i < s->len) {
        uint32_t seq[3];
        size_t n;
        if (s->codepoints[i] == 0x03A3 &&
            js_is_final_sigma32(s->codepoints, s->len, i)) {
            seq[0] = 0x03C2;
            n = 1;
        } else {
            n = JS_LOCALE_TO_LOWER_FULL_L(s->codepoints[i], locale, seq);
        }
        if (n == 1) {
            s->codepoints[i] = seq[0];
            i++;
        } else {
            if (s->len + (n - 1) > s->cap)
                break;
            memmove(s->codepoints + i + n,
                    s->codepoints + i + 1,
                    (s->len - i - 1) * sizeof(uint32_t));
            for (size_t j = 0; j < n; j++)
                s->codepoints[i + j] = seq[j];
            s->len += n - 1;
            i += n;
        }
    }
}

void jsstr32_toupper(jsstr32_t *s) {
    size_t i = 0;
    while (i < s->len) {
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_UPPER_FULL(s->codepoints[i], seq);
        if (n == 1) {
            s->codepoints[i] = seq[0];
            i++;
        } else {
            if (s->len + (n - 1) > s->cap)
                break;
            memmove(s->codepoints + i + n,
                    s->codepoints + i + 1,
                    (s->len - i - 1) * sizeof(uint32_t));
            for (size_t j = 0; j < n; j++)
                s->codepoints[i + j] = seq[j];
            s->len += n - 1;
            i += n;
        }
    }
}

void jsstr32_toupper_locale(jsstr32_t *s, const char *locale) {
    size_t i = 0;
    while (i < s->len) {
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_UPPER_FULL_L(s->codepoints[i], locale, seq);
        if (n == 1) {
            s->codepoints[i] = seq[0];
            i++;
        } else {
            if (s->len + (n - 1) > s->cap)
                break;
            memmove(s->codepoints + i + n,
                    s->codepoints + i + 1,
                    (s->len - i - 1) * sizeof(uint32_t));
            for (size_t j = 0; j < n; j++)
                s->codepoints[i + j] = seq[j];
            s->len += n - 1;
            i += n;
        }
    }
}

void jsstr32_u32_normalize(jsstr32_t *s) {
    s->len = JS_LOCALE_NORMALIZE_U32(s->codepoints, (int32_t)s->len, (int32_t)s->cap);
}

int jsstr32_u32_locale_compare(jsstr32_t *s1, jsstr32_t *s2) {
    return JS_LOCALE_COMPARE_U32(s1->codepoints, (int32_t)s1->len,
                                 s2->codepoints, (int32_t)s2->len);
}

int jsstr32_repeat(jsstr32_t *dest, jsstr32_t *src, size_t count) {
    if (src->len * count > dest->cap) {
        errno = ENOBUFS;
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        memcpy(dest->codepoints + i * src->len,
               src->codepoints,
               src->len * sizeof(uint32_t));
    }
    dest->len = src->len * count;
    return 0;
}

void jsstr32_pad_start(jsstr32_t *s, size_t target_len) {
    if (s->len >= target_len) {
        return;
    }
    size_t add = target_len - s->len;
    if (target_len > s->cap) {
        add = s->cap - s->len;
    }
    if (add == 0) {
        return;
    }
    memmove(s->codepoints + add, s->codepoints, s->len * sizeof(uint32_t));
    for (size_t i = 0; i < add; i++) {
        s->codepoints[i] = 0x20;
    }
    s->len += add;
}

void jsstr32_pad_end(jsstr32_t *s, size_t target_len) {
    if (s->len >= target_len) {
        return;
    }
    size_t add = target_len - s->len;
    if (target_len > s->cap) {
        add = s->cap - s->len;
    }
    for (size_t i = 0; i < add; i++) {
        s->codepoints[s->len + i] = 0x20;
    }
    s->len += add;
}

void jsstr32_trim_start(jsstr32_t *s) {
    size_t i = 0;
    while (i < s->len && JS_LOCALE_IS_SPACE(s->codepoints[i])) {
        i++;
    }
    if (i > 0) {
        memmove(s->codepoints, s->codepoints + i, (s->len - i) * sizeof(uint32_t));
        s->len -= i;
    }
}

void jsstr32_trim_end(jsstr32_t *s) {
    while (s->len > 0 && JS_LOCALE_IS_SPACE(s->codepoints[s->len - 1])) {
        s->len--;
    }
}

void jsstr32_trim(jsstr32_t *s) {
    jsstr32_trim_start(s);
    jsstr32_trim_end(s);
}

int jsstr32_u32_startswith(jsstr32_t *s, jsstr32_t *prefix) {
    if (prefix->len > s->len) {
        return 0;
    }
    for (size_t i = 0; i < prefix->len; i++) {
        if (s->codepoints[i] != prefix->codepoints[i]) {
            return 0;
        }
    }
    return 1;
}

int jsstr32_u32_endswith(jsstr32_t *s, jsstr32_t *suffix) {
    if (suffix->len > s->len) {
        return 0;
    }
    size_t start = s->len - suffix->len;
    for (size_t i = 0; i < suffix->len; i++) {
        if (s->codepoints[start + i] != suffix->codepoints[i]) {
            return 0;
        }
    }
    return 1;
}

int jsstr32_u32_includes(jsstr32_t *s, jsstr32_t *search) {
    if (search->len == 0) {
        return 1;
    }
    if (search->len > s->len) {
        return 0;
    }
    for (size_t i = 0; i <= s->len - search->len; i++) {
        size_t j;
        for (j = 0; j < search->len; j++) {
            if (s->codepoints[i + j] != search->codepoints[j]) {
                break;
            }
        }
        if (j == search->len) {
            return 1;
        }
    }
    return 0;
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

void jsstr16_init_from_str(jsstr16_t *s, const uint16_t *str) {
    size_t len = utf16_strlen(str);
    jsstr16_init_from_buf(s, (char *) str, len);
    s->len = len;
    return;
}

void jsstr16_u16_slice(jsstr16_t *s, jsstr16_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr16_u16s_at() to slice the buffer */
    s->codeunits = jsstr16_u16s_at(src, start_i);
    s->len = src->codeunits + src->len - s->codeunits;
    if (stop_i >= 0) {
        jsstr16_u16_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

void jsstr16_u32_slice(jsstr16_t *s, jsstr16_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr16_u32s_at() to slice the buffer */
    s->codeunits = jsstr16_u32s_at(src, start_i);
    s->len = src->codeunits + src->len - s->codeunits;
    if (stop_i >= 0) {
        jsstr16_u32_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr16_u16_cmp(jsstr16_t *s1, jsstr16_t *s2) {
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

int jsstr16_u32_cmp(jsstr16_t *s1, jsstr16_t *s2) {
    /* compare two jsstr16_t strings as UTF-32 code points */
    size_t i;
    for (i = 0; i < s1->len && i < s2->len; i++) {
        uint32_t c1, c2;
        int l1, l2;

        UTF16_CHAR(s1->codeunits + i, s1->codeunits + s1->len, &c1, &l1);
        UTF16_CHAR(s2->codeunits + i, s2->codeunits + s2->len, &c2, &l2);

        if (c1 != c2) {
            return c1 - c2;
        }
    }
    return (s1->len - s2->len);
}

ssize_t jsstr16_u16_indexof(jsstr16_t *s, uint16_t search_c, size_t start_i) {
    /* search for a code unit in the string, and return the code unit index (as works with _at() method) */
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && s->codeunits[i] == search_c) {
            return i;
        }
    }
    return -i;
}

ssize_t jsstr16_u32_indexof(jsstr16_t *s, uint32_t search_c, size_t start_i) {
    size_t code16_i;
    ssize_t cp_i;
    uint32_t c;
    int l;

    for (code16_i = 0, cp_i = 0; code16_i < s->len; cp_i++) {
        /* decode next UTF-16 code point */
        UTF16_CHAR(s->codeunits + code16_i,
                   s->codeunits + s->len,
                   &c, &l);

        if (l <= 0) {
            /* invalid sequence: advance one code unit as replacement */
            l = 1;
            c = 0xFFFD;
        }

        /* check match once we've reached start_i */
        if (cp_i >= (ssize_t)start_i && c == search_c) {
            return cp_i;
        }

        code16_i += l;
    }

    return -cp_i;
}

ssize_t jsstr16_u16_indextoken(jsstr16_t *s, uint16_t *search_c, size_t c_len, size_t start_i) {
    /* search for a code unit in the string, and return the code unit index (as works with _at() method) */
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && i + c_len <= s->len) {
            int j;
            for (j = 0; j < c_len; j++) {
                if (s->codeunits[i] == search_c[j]) {
                    return i;
                }
            }
        }
    }
    return -i;
}

ssize_t jsstr16_u32_indextoken(jsstr16_t *s, uint32_t *search_c, size_t c_len, size_t start_i) {
    /* search for a code point in the string, using UTF16_CHAR to decode code points */
    size_t code16_i;
    ssize_t cp_i;
    uint32_t c;
    int l;

    for (code16_i = 0, cp_i = 0; code16_i < s->len; cp_i++) {
        UTF16_CHAR(s->codeunits + code16_i, s->codeunits + s->len, &c, &l);
        if (l <= 0) {
            /* invalid sequence: advance one code unit as replacement */
            l = 1;
            c = 0xFFFD;
        }
        if (cp_i >= (ssize_t)start_i) {
            for (size_t j = 0; j < c_len; j++) {
                if (search_c[j] == c) {
                    return cp_i;
                }
            }
        }
        code16_i += l;
    }
    return -cp_i;
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
                if (j + 1 < s->cap) {
                    /* surrogate pair */
                    UTF16_CODEPAIR(c, s->codeunits + j);
                    j += 2; /* surrogate pair */
                } else {
                    break; /* not enough capacity */
                }
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
    return i;
}

size_t jsstr16_set_from_jsstr32(jsstr16_t *s, jsstr32_t *src) {
    /* convert a jsstr32_t string to a jsstr16_t string */
    return jsstr16_set_from_utf32(s, src->codepoints, src->len);
}

size_t jsstr16_set_from_jsstr8(jsstr16_t *s, jsstr8_t *src) {
    /* convert a jsstr8_t string to a jsstr16_t string */
    return jsstr16_set_from_utf8(s, src->bytes, src->len);
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

int jsstr16_is_well_formed(jsstr16_t *s) {
    /* check if the string is well-formed UTF-16 */
    return UTF16_WELL_FORMED(s->codeunits, s->codeunits + s->len);
}

void jsstr16_to_well_formed(jsstr16_t *s) {
    UTF16_TO_WELL_FORMED(s->codeunits, s->codeunits + s->len);
}

uint16_t *jsstr16_u16s_at(jsstr16_t *s, size_t i) {
    /* return the code unit at the index, or NULL if out of bounds */
    if (i < s->len) {
        return s->codeunits + i;
    } else {
        return NULL; /* out of bounds */
    }
}

uint16_t *jsstr16_u32s_at(jsstr16_t *s, size_t index) {
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

void jsstr16_u16_truncate(jsstr16_t *s, size_t len) {
    uint16_t *cu = jsstr16_u16s_at(s, len);
    if (cu != NULL) {
        s->len = cu - s->codeunits;
    }
}

void jsstr16_u32_truncate(jsstr16_t *s, size_t len) {
    /* truncate the string to the given code point length */
    uint16_t *cu = jsstr16_u32s_at(s, len);
    if (cu != NULL) {
        s->len = cu - s->codeunits;
    }
}

int jsstr16_u16_startswith(jsstr16_t *s, jsstr16_t *prefix) {
    if (prefix->len > s->len) {
        return 0;
    }
    for (size_t i = 0; i < prefix->len; i++) {
        if (s->codeunits[i] != prefix->codeunits[i]) {
            return 0;
        }
    }
    return 1;
}

int jsstr16_u16_endswith(jsstr16_t *s, jsstr16_t *suffix) {
    if (suffix->len > s->len) {
        return 0;
    }
    size_t start = s->len - suffix->len;
    for (size_t i = 0; i < suffix->len; i++) {
        if (s->codeunits[start + i] != suffix->codeunits[i]) {
            return 0;
        }
    }
    return 1;
}

int jsstr16_u16_includes(jsstr16_t *s, jsstr16_t *search) {
    if (search->len == 0) {
        return 1;
    }
    if (search->len > s->len) {
        return 0;
    }
    for (size_t i = 0; i <= s->len - search->len; i++) {
        size_t j;
        for (j = 0; j < search->len; j++) {
            if (s->codeunits[i + j] != search->codeunits[j]) {
                break;
            }
        }
        if (j == search->len) {
            return 1;
        }
    }
    return 0;
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


void jsstr16_u16_normalize(jsstr16_t *s) {
    s->len = JS_LOCALE_NORMALIZE_U16(s->codeunits, (int32_t)s->len, (int32_t)s->cap);
}

int jsstr16_u16_locale_compare(jsstr16_t *s1, jsstr16_t *s2) {
    return JS_LOCALE_COMPARE_U16(s1->codeunits, (int32_t)s1->len,
                                 s2->codeunits, (int32_t)s2->len);
}

void jsstr16_tolower(jsstr16_t *s) {
    uint16_t *end = s->codeunits + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF16_CHAR(s->codeunits + i, end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_LOWER_FULL(c, seq);
        int outlen = 0;
        for (size_t j = 0; j < n; j++)
            outlen += UTF16_CLEN(seq[j]);
        if (n == 1 && outlen == l) {
            if (outlen == 1)
                s->codeunits[i] = (uint16_t)seq[0];
            else
                UTF16_CODEPAIR(seq[0], s->codeunits + i);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->codeunits + i + outlen,
                    s->codeunits + i + l,
                    (s->len - i - l) * sizeof(uint16_t));
            uint16_t tmp[6];
            uint16_t *p = tmp;
            for (size_t j = 0; j < n; j++) {
                if (UTF16_CLEN(seq[j]) == 1) {
                    *p++ = (uint16_t)seq[j];
                } else {
                    UTF16_CODEPAIR(seq[j], p);
                    p += 2;
                }
            }
            memcpy(s->codeunits + i, tmp, outlen * sizeof(uint16_t));
            s->len += outlen - l;
            end = s->codeunits + s->len;
            i += outlen;
        }
    }
}

void jsstr16_tolower_locale(jsstr16_t *s, const char *locale) {
    uint16_t *end = s->codeunits + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF16_CHAR(s->codeunits + i, end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_LOWER_FULL_L(c, locale, seq);
        int outlen = 0;
        for (size_t j = 0; j < n; j++)
            outlen += UTF16_CLEN(seq[j]);
        if (n == 1 && outlen == l) {
            if (outlen == 1)
                s->codeunits[i] = (uint16_t)seq[0];
            else
                UTF16_CODEPAIR(seq[0], s->codeunits + i);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->codeunits + i + outlen,
                    s->codeunits + i + l,
                    (s->len - i - l) * sizeof(uint16_t));
            uint16_t tmp[6];
            uint16_t *p = tmp;
            for (size_t j = 0; j < n; j++) {
                if (UTF16_CLEN(seq[j]) == 1) {
                    *p++ = (uint16_t)seq[j];
                } else {
                    UTF16_CODEPAIR(seq[j], p);
                    p += 2;
                }
            }
            memcpy(s->codeunits + i, tmp, outlen * sizeof(uint16_t));
            s->len += outlen - l;
            end = s->codeunits + s->len;
            i += outlen;
        }
    }
}

void jsstr16_toupper(jsstr16_t *s) {
    uint16_t *end = s->codeunits + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF16_CHAR(s->codeunits + i, end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_UPPER_FULL(c, seq);
        int outlen = 0;
        for (size_t j = 0; j < n; j++)
            outlen += UTF16_CLEN(seq[j]);
        if (n == 1 && outlen == l) {
            if (outlen == 1)
                s->codeunits[i] = (uint16_t)seq[0];
            else
                UTF16_CODEPAIR(seq[0], s->codeunits + i);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->codeunits + i + outlen,
                    s->codeunits + i + l,
                    (s->len - i - l) * sizeof(uint16_t));
            uint16_t tmp[6];
            uint16_t *p = tmp;
            for (size_t j = 0; j < n; j++) {
                if (UTF16_CLEN(seq[j]) == 1) {
                    *p++ = (uint16_t)seq[j];
                } else {
                    UTF16_CODEPAIR(seq[j], p);
                    p += 2;
                }
            }
            memcpy(s->codeunits + i, tmp, outlen * sizeof(uint16_t));
            s->len += outlen - l;
            end = s->codeunits + s->len;
            i += outlen;
        }
    }
}

void jsstr16_toupper_locale(jsstr16_t *s, const char *locale) {
    uint16_t *end = s->codeunits + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF16_CHAR(s->codeunits + i, end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_UPPER_FULL_L(c, locale, seq);
        int outlen = 0;
        for (size_t j = 0; j < n; j++)
            outlen += UTF16_CLEN(seq[j]);
        if (n == 1 && outlen == l) {
            if (outlen == 1)
                s->codeunits[i] = (uint16_t)seq[0];
            else
                UTF16_CODEPAIR(seq[0], s->codeunits + i);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->codeunits + i + outlen,
                    s->codeunits + i + l,
                    (s->len - i - l) * sizeof(uint16_t));
            uint16_t tmp[6];
            uint16_t *p = tmp;
            for (size_t j = 0; j < n; j++) {
                if (UTF16_CLEN(seq[j]) == 1) {
                    *p++ = (uint16_t)seq[j];
                } else {
                    UTF16_CODEPAIR(seq[j], p);
                    p += 2;
                }
            }
            memcpy(s->codeunits + i, tmp, outlen * sizeof(uint16_t));
            s->len += outlen - l;
            end = s->codeunits + s->len;
            i += outlen;
        }
    }
}

void jsstr16_u32_normalize(jsstr16_t *s) {
    s->len = JS_LOCALE_NORMALIZE_U16(s->codeunits, (int32_t)s->len, (int32_t)s->cap);
}

int jsstr16_u32_locale_compare(jsstr16_t *s1, jsstr16_t *s2) {
    return JS_LOCALE_COMPARE_U16(s1->codeunits, (int32_t)s1->len,
                                 s2->codeunits, (int32_t)s2->len);
}

int jsstr16_repeat(jsstr16_t *dest, jsstr16_t *src, size_t count) {
    if (src->len * count > dest->cap) {
        errno = ENOBUFS;
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        memcpy(dest->codeunits + i * src->len,
               src->codeunits,
               src->len * sizeof(uint16_t));
    }
    dest->len = src->len * count;
    return 0;
}

void jsstr16_pad_start(jsstr16_t *s, size_t target_len) {
    if (s->len >= target_len) {
        return;
    }
    size_t add = target_len - s->len;
    if (target_len > s->cap) {
        add = s->cap - s->len;
    }
    if (add == 0) {
        return;
    }
    memmove(s->codeunits + add, s->codeunits, s->len * sizeof(uint16_t));
    for (size_t i = 0; i < add; i++) {
        s->codeunits[i] = 0x20;
    }
    s->len += add;
}

void jsstr16_pad_end(jsstr16_t *s, size_t target_len) {
    if (s->len >= target_len) {
        return;
    }
    size_t add = target_len - s->len;
    if (target_len > s->cap) {
        add = s->cap - s->len;
    }
    for (size_t i = 0; i < add; i++) {
        s->codeunits[s->len + i] = 0x20;
    }
    s->len += add;
}

void jsstr16_trim_start(jsstr16_t *s) {
    size_t i = 0;
    while (i < s->len && JS_LOCALE_IS_SPACE(s->codeunits[i])) {
        i++;
    }
    if (i > 0) {
        memmove(s->codeunits, s->codeunits + i, (s->len - i) * sizeof(uint16_t));
        s->len -= i;
    }
}

void jsstr16_trim_end(jsstr16_t *s) {
    while (s->len > 0 && JS_LOCALE_IS_SPACE(s->codeunits[s->len - 1])) {
        s->len--;
    }
}

void jsstr16_trim(jsstr16_t *s) {
    jsstr16_trim_start(s);
    jsstr16_trim_end(s);
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

void jsstr8_init_from_str(jsstr8_t *s, const char *str) {
    size_t len = strlen(str);
    jsstr8_init_from_buf(s, str, len);
    s->len = len;
    return;
}

void jsstr8_u8_slice(jsstr8_t *s, jsstr8_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr8_u8s_at() to slice the buffer */
    s->bytes = jsstr8_u8s_at(src, start_i);
    s->len = src->bytes + src->len - s->bytes;
    if (stop_i >= 0) {
        jsstr8_u8_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

void jsstr8_u32_slice(jsstr8_t *s, jsstr8_t *src, size_t start_i, ssize_t stop_i) {
    /* initialize from a slice of a source string, using jsstr8_u32s_at() to slice the buffer */
    s->bytes = jsstr8_u32s_at(src, start_i);
    s->len = src->bytes + src->len - s->bytes;
    if (stop_i >= 0) {
        jsstr8_u32_truncate(s, stop_i - start_i);
    }
    s->cap = s->len;
}

int jsstr8_u8_cmp(jsstr8_t *s1, jsstr8_t *s2) {
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

int jsstr8_u32_cmp(jsstr8_t *s1, jsstr8_t *s2) {
    /* compare two strings as UTF-32 code points */
    if (s1->len != s2->len) {
        return s1->len - s2->len;
    }
    uint32_t c1, c2;
    int l1, l2;
    size_t i = 0;
    while (i < s1->len && i < s2->len) {
        UTF8_CHAR((const char *) s1->bytes + i, (const char *) s1->bytes + s1->len, &c1, &l1);
        UTF8_CHAR((const char *) s2->bytes + i, (const char *) s2->bytes + s2->len, &c2, &l2);
        if (c1 != c2) {
            return c1 - c2;
        }
        i += l1 > l2 ? l1 : l2; /* advance by the longer length */
    }
    return 0; /* equal */
}

ssize_t jsstr8_u8_indexof(jsstr8_t *s, uint8_t search_c, size_t start_i) {
    /* search for a byte in the string, and return the byte index (as works with _at() method) */
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && s->bytes[i] == search_c) {
            return i;
        }
    }
    return -i;
}

ssize_t jsstr8_u32_indexof(jsstr8_t *s, uint32_t search_c, size_t start_i) {
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

ssize_t jsstr8_u8_indextoken(jsstr8_t *s, uint8_t *search_c, size_t c_len, size_t start_i) {
    /* search for a byte in the string, and return the byte index (as works with _at() method) */
    ssize_t i;
    for (i = 0; i < s->len; i++) {
        if (i >= start_i && i + c_len <= s->len) {
            int j;
            for (j = 0; j < c_len; j++) {
                if (s->bytes[i] == search_c[j]) {
                    return i;
                }
            }
        }
    }
    return -i;
}

ssize_t jsstr8_u32_indextoken(jsstr8_t *s, uint32_t *search_c, size_t c_len, size_t start_i) {
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

int jsstr8_u8_startswith(jsstr8_t *s, jsstr8_t *prefix) {
    if (prefix->len > s->len) {
        return 0;
    }
    for (size_t i = 0; i < prefix->len; i++) {
        if (s->bytes[i] != prefix->bytes[i]) {
            return 0;
        }
    }
    return 1;
}

int jsstr8_u8_endswith(jsstr8_t *s, jsstr8_t *suffix) {
    if (suffix->len > s->len) {
        return 0;
    }
    size_t start = s->len - suffix->len;
    for (size_t i = 0; i < suffix->len; i++) {
        if (s->bytes[start + i] != suffix->bytes[i]) {
            return 0;
        }
    }
    return 1;
}

int jsstr8_u8_includes(jsstr8_t *s, jsstr8_t *search) {
    if (search->len == 0) {
        return 1;
    }
    if (search->len > s->len) {
        return 0;
    }
    for (size_t i = 0; i <= s->len - search->len; i++) {
        size_t j;
        for (j = 0; j < search->len; j++) {
            if (s->bytes[i + j] != search->bytes[j]) {
                break;
            }
        }
        if (j == search->len) {
            return 1;
        }
    }
    return 0;
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

size_t jsstr8_set_from_utf16(jsstr8_t *s, const uint16_t *str, size_t len) {
    /* decode each UTF-16 code unit into a UTF-8 byte sequence, assigning into bytes array */
    size_t i = 0;
    size_t j = 0;
    while (i < len && j < s->cap) {
        uint32_t c;
        const uint32_t *cc = &c;
        uint8_t *bc = s->bytes + j;
        uint8_t *bs = s->bytes + s->cap;
        int l;
        UTF16_CHAR(str + i, str + len, &c, &l);
        if (l > 0) {
            if (j + l <= s->cap) {
                UTF8_ENCODE(&cc, cc + 1, &bc, bs);
                j += l; /* advance by the length of the UTF-8 sequence */
            } else {
                break; /* not enough capacity */
            }
        } else {
            break; /* invalid character */
        }
        i++;
    }
    s->len = j;
    return i; /* return the number of code units processed */
}

size_t jsstr8_set_from_utf32(jsstr8_t *s, const uint32_t *str, size_t len) {
    /* decode each wide character into a UTF-8 byte sequence, assigning into bytes array */
    size_t i = 0;
    size_t j = 0;
    while (i < len && j < s->cap) {
        uint32_t c;
        const  uint32_t *cc = str + i;
        uint8_t *cb = s->bytes + j;
        uint8_t *cs = s->bytes + s->cap;
        int l;
        c = str[i];
        l = UTF8_CLEN(c);
        if (l > 0) {
            if (j + l <= s->cap) {
                UTF8_ENCODE(&cc, cc + 1, &cb, cs);
                j += l; /* advance by the length of the UTF-8 sequence */
            } else {
                break; /* not enough capacity */
            }
        } else {
            break; /* invalid character */
        }
        i++;
    }
    s->len = j;
    return i; /* return the number of code points processed */
}

size_t jsstr8_set_from_jsstr16(jsstr8_t *s, jsstr16_t *src) {
    /* convert a jsstr16_t string to a jsstr8_t string */
    return jsstr8_set_from_utf16(s, src->codeunits, src->len);
}

size_t jsstr8_set_from_jsstr32(jsstr8_t *s, jsstr32_t *src) {
    /* convert a jsstr32_t string to a jsstr8_t string */
    return jsstr8_set_from_utf32(s, src->codepoints, src->len);
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

int jsstr8_is_well_formed(jsstr8_t *s) {
    /* check if the string is well-formed UTF-8 */
    return UTF8_WELL_FORMED(s->bytes, s->bytes + s->len);
}

size_t jsstr8_to_well_formed(jsstr8_t *s, jsstr8_t *dest) {
    return dest->len = UTF8_TO_WELL_FORMED((const uint8_t *) s->bytes, s->bytes + s->len,
                      (uint8_t *) dest->bytes, dest->cap);
}

uint8_t *jsstr8_u8s_at(jsstr8_t *s, size_t i) {
    /* return the byte at the index, or NULL if out of bounds */
    if (i < s->len) {
        return s->bytes + i;
    } else {
        return NULL; /* out of bounds */
    }
}

uint8_t *jsstr8_u32s_at(jsstr8_t *s, size_t i) {
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

void jsstr8_u8_truncate(jsstr8_t *s, size_t len) {
    /* truncate the string to the given byte length */
    uint8_t *bc = jsstr8_u8s_at(s, len);
    if (bc != NULL) {
        s->len = bc - s->bytes;
    }
}

void jsstr8_u32_truncate(jsstr8_t *s, size_t len) {
    uint8_t *bc = jsstr8_u32s_at(s, len);
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


void jsstr8_tolower(jsstr8_t *s) {
    uint8_t *end = s->bytes + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *)s->bytes + i, (const char *)end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_LOWER_FULL(c, seq);
        uint8_t buf[12];
        uint8_t *p = buf;
        for (size_t j = 0; j < n; j++) {
            const uint32_t *cc = &seq[j];
            UTF8_ENCODE(&cc, &seq[j] + 1, &p, buf + sizeof(buf));
        }
        size_t outlen = (size_t)(p - buf);
        if (n == 1 && outlen == (size_t)l) {
            memcpy(s->bytes + i, buf, outlen);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->bytes + i + outlen,
                    s->bytes + i + l,
                    s->len - i - l);
            memcpy(s->bytes + i, buf, outlen);
            s->len += outlen - l;
            end = s->bytes + s->len;
            i += outlen;
        }
    }
}

void jsstr8_tolower_locale(jsstr8_t *s, const char *locale) {
    uint8_t *end = s->bytes + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *)s->bytes + i, (const char *)end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_LOWER_FULL_L(c, locale, seq);
        uint8_t buf[12];
        uint8_t *p = buf;
        for (size_t j = 0; j < n; j++) {
            const uint32_t *cc = &seq[j];
            UTF8_ENCODE(&cc, &seq[j] + 1, &p, buf + sizeof(buf));
        }
        size_t outlen = (size_t)(p - buf);
        if (n == 1 && outlen == (size_t)l) {
            memcpy(s->bytes + i, buf, outlen);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->bytes + i + outlen,
                    s->bytes + i + l,
                    s->len - i - l);
            memcpy(s->bytes + i, buf, outlen);
            s->len += outlen - l;
            end = s->bytes + s->len;
            i += outlen;
        }
    }
}

void jsstr8_toupper(jsstr8_t *s) {
    uint8_t *end = s->bytes + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *)s->bytes + i, (const char *)end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_UPPER_FULL(c, seq);
        uint8_t buf[12];
        uint8_t *p = buf;
        for (size_t j = 0; j < n; j++) {
            const uint32_t *cc = &seq[j];
            UTF8_ENCODE(&cc, &seq[j] + 1, &p, buf + sizeof(buf));
        }
        size_t outlen = (size_t)(p - buf);
        if (n == 1 && outlen == (size_t)l) {
            memcpy(s->bytes + i, buf, outlen);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->bytes + i + outlen,
                    s->bytes + i + l,
                    s->len - i - l);
            memcpy(s->bytes + i, buf, outlen);
            s->len += outlen - l;
            end = s->bytes + s->len;
            i += outlen;
        }
    }
}

void jsstr8_toupper_locale(jsstr8_t *s, const char *locale) {
    uint8_t *end = s->bytes + s->len;
    size_t i = 0;
    while (i < s->len) {
        uint32_t c;
        int l;
        UTF8_CHAR((const char *)s->bytes + i, (const char *)end, &c, &l);
        if (l <= 0) { l = l ? -l : 1; c = 0xFFFD; }
        uint32_t seq[3];
        size_t n = JS_LOCALE_TO_UPPER_FULL_L(c, locale, seq);
        uint8_t buf[12];
        uint8_t *p = buf;
        for (size_t j = 0; j < n; j++) {
            const uint32_t *cc = &seq[j];
            UTF8_ENCODE(&cc, &seq[j] + 1, &p, buf + sizeof(buf));
        }
        size_t outlen = (size_t)(p - buf);
        if (n == 1 && outlen == (size_t)l) {
            memcpy(s->bytes + i, buf, outlen);
            i += outlen;
        } else {
            if (s->len + outlen - l > s->cap)
                break;
            memmove(s->bytes + i + outlen,
                    s->bytes + i + l,
                    s->len - i - l);
            memcpy(s->bytes + i, buf, outlen);
            s->len += outlen - l;
            end = s->bytes + s->len;
            i += outlen;
        }
    }
}

void jsstr8_u8_normalize(jsstr8_t *s) {
    s->len = JS_LOCALE_NORMALIZE_U8(s->bytes, (int32_t)s->len, (int32_t)s->cap);
}

void jsstr8_u32_normalize(jsstr8_t *s) {
    s->len = JS_LOCALE_NORMALIZE_U8(s->bytes, (int32_t)s->len, (int32_t)s->cap);
}

int jsstr8_u8_locale_compare(jsstr8_t *s1, jsstr8_t *s2) {
    return JS_LOCALE_COMPARE_U8(s1->bytes, (int32_t)s1->len,
                                s2->bytes, (int32_t)s2->len);
}

int jsstr8_u32_locale_compare(jsstr8_t *s1, jsstr8_t *s2) {
    return JS_LOCALE_COMPARE_U8(s1->bytes, (int32_t)s1->len,
                                s2->bytes, (int32_t)s2->len);
}

int jsstr8_repeat(jsstr8_t *dest, jsstr8_t *src, size_t count) {
    if (src->len * count > dest->cap) {
        errno = ENOBUFS;
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        memcpy(dest->bytes + i * src->len,
               src->bytes,
               src->len);
    }
    dest->len = src->len * count;
    return 0;
}

void jsstr8_pad_start(jsstr8_t *s, size_t target_len) {
    if (s->len >= target_len) {
        return;
    }
    size_t add = target_len - s->len;
    if (target_len > s->cap) {
        add = s->cap - s->len;
    }
    if (add == 0) {
        return;
    }
    memmove(s->bytes + add, s->bytes, s->len);
    memset(s->bytes, ' ', add);
    s->len += add;
}

void jsstr8_pad_end(jsstr8_t *s, size_t target_len) {
    if (s->len >= target_len) {
        return;
    }
    size_t add = target_len - s->len;
    if (target_len > s->cap) {
        add = s->cap - s->len;
    }
    memset(s->bytes + s->len, ' ', add);
    s->len += add;
}

void jsstr8_trim_start(jsstr8_t *s) {
    size_t i = 0;
    while (i < s->len && JS_LOCALE_IS_SPACE((uint32_t)s->bytes[i])) {
        i++;
    }
    if (i > 0) {
        memmove(s->bytes, s->bytes + i, s->len - i);
        s->len -= i;
    }
}

void jsstr8_trim_end(jsstr8_t *s) {
    while (s->len > 0 && JS_LOCALE_IS_SPACE((uint32_t)s->bytes[s->len - 1])) {
        s->len--;
    }
}

void jsstr8_trim(jsstr8_t *s) {
    jsstr8_trim_start(s);
    jsstr8_trim_end(s);
}
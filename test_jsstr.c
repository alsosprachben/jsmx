#include <stdio.h>
#include <string.h>
#include "jsstr.h"
#include "utf8.h"
#include <locale.h>



void test_jsstr32_lifecycle() {
    jsstr32_t s;
    jsstr32_t s_slice;
    jsstr32_t dest;
    uint8_t buf[1024];
    size_t len = sizeof(buf);
    uint8_t dest_buf[1024];
    size_t dest_len = sizeof(dest_buf);
    size_t rc;
    const uint32_t *cu;
    uint32_t c;
    uint32_t *c_ptr = &c;

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81";
    size_t u8_strlen = utf8_strlen(utf8_str);
    uint16_t utf16_str[] = {
        0x0048,0x0065,0x006C,0x006C,0x006F,0x002C,0x0020,
        0x0057,0x006F,0x0072,0x006C,0x0064,0x0021,0
    };
    size_t u16_strlen = sizeof(utf16_str)/sizeof(utf16_str[0]) - 1;
    uint32_t *utf32_str = L"Hello, World! 😀";
    size_t u32_strlen = utf32_strlen(utf32_str);

    printf("utf8_strlen: %zu\n", u8_strlen);
    printf("utf16_strlen: %zu\n", u16_strlen);
    printf("utf32_strlen: %zu\n", u32_strlen);

    jsstr32_init_from_buf(&s, buf, len);
    printf("jsstr32_init_from_buf: %zu\n", jsstr32_get_cap(&s));

    rc = jsstr32_set_from_utf8(&s, utf8_str, u8_strlen);
    printf("jsstr32_set_from_utf8: %zu\n", rc);
    printf("jsstr32_get_utf8len: %zu\n", jsstr32_get_utf8len(&s));

    cu = jsstr32_u32s_at(&s, 20);
    if (cu == NULL) {
        printf("jsstr32_u32s_at: NULL\n");
    } else {
        printf("jsstr32_u32s_at: %lc\n", *cu);
    }

    rc = jsstr32_set_from_utf16(&s, utf16_str, u16_strlen);
    printf("jsstr32_set_from_utf16: %zu\n", rc);
    printf("jsstr32_get_utf16len: %zu\n", jsstr32_get_utf16len(&s));

    cu = jsstr32_u32s_at(&s, 20);
    if (cu == NULL) {
        printf("jsstr32_u32s_at: NULL\n");
    } else {
        printf("jsstr32_u32s_at: %lc\n", *cu);
    }

    rc = jsstr32_set_from_utf32(&s, utf32_str, u32_strlen);
    printf("jsstr32_set_from_utf32: %zu\n", rc);
    printf("jsstr32_get_utf32len: %zu\n", jsstr32_get_utf32len(&s));

    cu = jsstr32_u32s_at(&s, 20);
    if (cu == NULL) {
        printf("jsstr32_u32s_at: NULL\n");
    } else {
        printf("jsstr32_u32s_at: %lc\n", *cu);
    }

    jsstr32_u32_truncate(&s, 10);
    printf("jsstr32_u32_truncate: %zu\n", jsstr32_get_utf32len(&s));

    jsstr32_u32_slice(&s_slice, &s, 3, 7);
    printf("jsstr32_u32_slice: %zu\n", jsstr32_get_utf32len(&s_slice));

    jsstr32_init_from_str(&s, L"Hello, World!");
    printf("jsstr32_init_from_str: %zu\n", jsstr32_get_utf32len(&s));

    jsstr32_t s_cmp;
    jsstr32_init_from_str(&s_cmp, L"Hello, World!");
    printf("jsstr32_u32_cmp: %d\n", jsstr32_u32_cmp(&s, &s_cmp));

    uint32_t search_c = L'o';
    size_t start_i = 6;
    printf("jsstr32_u32_indexof: %zd\n", jsstr32_u32_indexof(&s, search_c, start_i));

    uint32_t *search_tok = L"orld";
    size_t search_tok_len = utf32_strlen(search_tok);
    printf("jsstr32_u32_indextoken: %zd\n",
           jsstr32_u32_indextoken(&s, search_tok, search_tok_len, start_i));

    jsstr32_t prefix;
    jsstr32_t suffix;
    jsstr32_t sub;
    jsstr32_init_from_str(&prefix, L"Hello");
    jsstr32_init_from_str(&suffix, L"World!");
    jsstr32_init_from_str(&sub, L", Wo");
    printf("jsstr32_u32_startswith: %d\n", jsstr32_u32_startswith(&s, &prefix));
    printf("jsstr32_u32_endswith: %d\n", jsstr32_u32_endswith(&s, &suffix));
    printf("jsstr32_u32_includes: %d\n", jsstr32_u32_includes(&s, &sub));

    jsstr32_init_from_buf(&s, (char *)buf, len);
    jsstr32_set_from_utf32(&s, L"abc", 3);
    jsstr32_u32_toupper(&s);
    printf("jsstr32_toupper: %lc%lc%lc\n", s.codepoints[0], s.codepoints[1], s.codepoints[2]);
    jsstr32_u32_tolower(&s);
    printf("jsstr32_tolower: %lc%lc%lc\n", s.codepoints[0], s.codepoints[1], s.codepoints[2]);
    jsstr32_init_from_buf(&dest, (char *)dest_buf, dest_len);
    jsstr32_repeat(&dest, &s, 3);
    printf("jsstr32_repeat len: %zu\n", jsstr32_get_utf32len(&dest));
}

void test_jsstr16_lifecycle() {
    jsstr16_t s;
    jsstr16_t s_str;
    jsstr16_t s_cmp;
    jsstr16_t s_slice;
    jsstr16_t dest16;
    uint8_t buf[1024];
    size_t len = sizeof (buf);
    uint8_t dest16_buf[1024];
    size_t dest16_len = sizeof(dest16_buf);
    size_t rc;
    const uint16_t *cu;
    uint32_t c;
    uint32_t *c_ptr = &c;


    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    size_t u8_strlen = utf8_strlen(utf8_str);
    uint16_t utf16_str[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F, 0x002C, 0x0020, 0x0057, 0x006F, 0x0072, 0x006C, 0x0064, 0x0021, 0}; // UTF-16 sequence for "Hello, World!"
    size_t u16_strlen = sizeof(utf16_str) / sizeof(utf16_str[0]) - 1;
    uint32_t *utf32_str = L"Hello, World! 😀";
    size_t u32_strlen = utf32_strlen(utf32_str);

    printf("utf8_strlen: %zu\n", u8_strlen);
    printf("utf16_strlen: %zu\n", u16_strlen);
    printf("utf32_strlen: %zu\n", u32_strlen);
    
    jsstr16_init_from_buf(&s, buf, len);
    printf("jsstr16_init_from_buf: %zu\n", jsstr16_get_cap(&s));

    rc = jsstr16_set_from_utf8(&s, utf8_str, u8_strlen);
    printf("jsstr16_set_from_utf8: %zu\n", rc);
    printf("jsstr16_get_utf8len: %zu\n", jsstr16_get_utf8len(&s));

    cu = jsstr16_u16s_at(&s, 20);
    if (cu == NULL) {
        printf("jsstr16_u16s_at: NULL\n");
    } else {
        c_ptr = &c;
        UTF16_DECODE(&cu, cu + 2, &c_ptr, c_ptr + 1);
        printf("jsstr16_u16s_at: %lc\n", c);
    }

    rc = jsstr16_set_from_utf16(&s, utf16_str, u16_strlen);
    printf("jsstr16_set_from_utf16: %zu\n", rc);
    printf("jsstr16_get_utf16len: %zu\n", jsstr16_get_utf16len(&s));

    cu = jsstr16_u16s_at(&s, 20);
    if (cu == NULL) {
        printf("jsstr16_u16s_at: NULL\n");
    } else {
        c_ptr = &c;
        UTF16_DECODE(&cu, cu + 2, &c_ptr, c_ptr + 1);
        printf("jsstr16_u16s_at: %lc\n", c);
    }

    rc = jsstr16_set_from_utf32(&s, utf32_str, u32_strlen);
    printf("jsstr16_set_from_utf32: %zu\n", rc);
    printf("jsstr16_get_utf32len: %zu\n", jsstr16_get_utf32len(&s));

    /* native u16 encoding ops */

    cu = jsstr16_u16s_at(&s, 20);
    if (cu == NULL) {
        printf("jsstr16_u16s_at: NULL\n");
    } else {
        c_ptr = &c;
        UTF16_DECODE(&cu, cu + 2, &c_ptr, c_ptr + 1);
        printf("jsstr16_u16s_at: %lc\n", c);
    }

    jsstr16_u16_truncate(&s, 10);
    printf("jsstr16_u16_truncate: %zu\n", jsstr16_get_utf32len(&s));

    jsstr16_u16_slice(&s_slice, &s, 3, 7);
    printf("jsstr16_u16_slice: %zu\n", jsstr16_get_utf32len(&s_slice));

    jsstr16_init_from_str(&s_str, utf16_str);
    printf("jsstr16_init_from_str: %zu\n", jsstr16_get_utf32len(&s_str));

    /* test cmp */
    jsstr16_init_from_str(&s_cmp, utf16_str);
    printf("jsstr16_u16_cmp: %d\n", jsstr16_u16_cmp(&s_str, &s_cmp));

    /* test indexof */
    uint32_t search_c = L'o';
    size_t start_i = 6;
    printf("jsstr16_u32_indexof: %zd\n", jsstr16_u32_indexof(&s_str, search_c, start_i));

    /* test indexof with utf16 */
    uint16_t search_c16 = 'o';
    printf("jsstr16_u16_indexof: %zd\n", jsstr16_u16_indexof(&s_str, search_c16, start_i));

    /* test indextoken */
    int32_t *search_c2i = L"orld";
    uint32_t *search_c2 = (uint32_t *) search_c2i;
    size_t search_c_len = utf32_strlen(search_c2);
    printf("jsstr16_u32_indextoken: %zd\n", jsstr16_u32_indextoken(&s_str, search_c2, search_c_len, start_i));

    /* test indextoken with utf16 */
    uint16_t search_c216[] = {0x006F, 0x0072, 0x006C, 0x0064, 0};
    size_t search_c216_len = utf16_strlen(search_c216);
    printf("jsstr16_u16_indextoken: %zd\n", jsstr16_u16_indextoken(&s_str, search_c216, search_c216_len, start_i));

    /* character u32 encoding ops */

    cu = jsstr16_u32s_at(&s_str, 20);
    if (cu == NULL) {
        printf("jsstr16_u32s_at: NULL\n");
    } else {
        c_ptr = &c;
        UTF16_DECODE(&cu, cu + 2, &c_ptr, c_ptr + 1);
        printf("jsstr16_u32s_at: %lc\n", c);
    }

    jsstr16_u32_truncate(&s, 10);
    printf("jsstr16_u32_truncate: %zu\n", jsstr16_get_utf32len(&s));

    jsstr16_u32_slice(&s_slice, &s_str, 3, 7);
    printf("jsstr16_u32_slice: %zu\n", jsstr16_get_utf32len(&s_slice));

    /* test cmp */
    jsstr16_init_from_str(&s_cmp, utf16_str);
    printf("jsstr16_u32_cmp: %d\n", jsstr16_u32_cmp(&s_str, &s_cmp));

    jsstr16_t prefix;
    jsstr16_t suffix;
    jsstr16_t sub;
    jsstr16_init_from_str(&prefix, (uint16_t *)L"Hello");
    jsstr16_init_from_str(&suffix, (uint16_t *)L"World!");
    jsstr16_init_from_str(&sub, (uint16_t *)L", Wo");
    printf("jsstr16_u16_startswith: %d\n", jsstr16_u16_startswith(&s_str, &prefix));
    printf("jsstr16_u16_endswith: %d\n", jsstr16_u16_endswith(&s_str, &suffix));
    printf("jsstr16_u16_includes: %d\n", jsstr16_u16_includes(&s_str, &sub));

    jsstr16_init_from_buf(&s, (char *)buf, len);
    {
        uint16_t tmp[3] = { 'a', 'b', 'c' };
        jsstr16_set_from_utf16(&s, tmp, 3);
    }
    jsstr16_u32_toupper(&s);
    printf("jsstr16_u32_toupper: %04x %04x %04x\n", s.codeunits[0], s.codeunits[1], s.codeunits[2]);
    jsstr16_u32_tolower(&s);
    printf("jsstr16_u32_tolower: %04x %04x %04x\n", s.codeunits[0], s.codeunits[1], s.codeunits[2]);
    jsstr16_u16_toupper(&s);
    printf("jsstr16_u16_toupper: %04x %04x %04x\n", s.codeunits[0], s.codeunits[1], s.codeunits[2]);
    jsstr16_u16_tolower(&s);
    printf("jsstr16_u16_tolower: %04x %04x %04x\n", s.codeunits[0], s.codeunits[1], s.codeunits[2]);
    jsstr16_init_from_buf(&dest16, (char *)dest16_buf, dest16_len);
    jsstr16_repeat(&dest16, &s, 3);
    printf("jsstr16_repeat len: %zu\n", jsstr16_get_utf16len(&dest16));
}

void test_jsstr8_lifecycle() {
    jsstr8_t s;
    jsstr8_t s_str;
    jsstr8_t s_cmp;
    jsstr8_t s_slice;
    jsstr8_t dest8;
    uint8_t buf[1024];
    size_t len = sizeof(buf);
    uint8_t dest8_buf[1024];
    size_t dest8_len = sizeof(dest8_buf);
    size_t rc;
    const uint8_t *u8;
    uint32_t c;
    uint32_t *c_ptr = &c;

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81";
    size_t u8_strlen = utf8_strlen(utf8_str);
    uint16_t utf16_str[] = {
        0x0048,0x0065,0x006C,0x006C,0x006F,0x002C,0x0020,
        0x0057,0x006F,0x0072,0x006C,0x0064,0x0021,0
    };
    size_t u16_strlen = sizeof(utf16_str)/sizeof(utf16_str[0]) - 1;
    uint32_t *utf32_str = L"Hello, World! 😀";
    size_t u32_strlen = utf32_strlen(utf32_str);

    printf("utf8_strlen: %zu\n", u8_strlen);
    printf("utf16_strlen: %zu\n", u16_strlen);
    printf("utf32_strlen: %zu\n", u32_strlen);

    jsstr8_init_from_buf(&s, buf, len);
    printf("jsstr8_init_from_buf: %zu\n", jsstr8_get_cap(&s));

    rc = jsstr8_set_from_utf8(&s, utf8_str, u8_strlen);
    printf("jsstr8_set_from_utf8: %zu\n", rc);
    printf("jsstr8_get_utf8len: %zu\n", jsstr8_get_utf8len(&s));

    u8 = jsstr8_u8s_at(&s, 20);
    if (u8 == NULL) {
        printf("jsstr8_u8s_at: NULL\n");
    } else {
        UTF8_DECODE(&u8, u8 + 4, &c_ptr, c_ptr + 1);
        printf("jsstr8_u8s_at: %lc\n", c);
    }

    rc = jsstr8_set_from_utf16(&s, utf16_str, u16_strlen);
    printf("jsstr8_set_from_utf16: %zu\n", rc);
    printf("jsstr8_get_utf16len: %zu\n", jsstr8_get_utf16len(&s));

    u8 = jsstr8_u8s_at(&s, 20);
    if (u8 == NULL) {
        printf("jsstr8_u8s_at: NULL\n");
    } else {
        UTF8_DECODE(&u8, u8 + 4, &c_ptr, c_ptr + 1);
        printf("jsstr8_u8s_at: %lc\n", c);
    }

    rc = jsstr8_set_from_utf32(&s, utf32_str, u32_strlen);
    printf("jsstr8_set_from_utf32: %zu\n", rc);
    printf("jsstr8_get_utf32len: %zu\n", jsstr8_get_utf32len(&s));

    u8 = jsstr8_u8s_at(&s, 20);
    if (u8 == NULL) {
        printf("jsstr8_u8s_at: NULL\n");
    } else {
        UTF8_DECODE(&u8, u8 + 4, &c_ptr, c_ptr + 1);
        printf("jsstr8_u8s_at: %lc\n", c);
    }

    jsstr8_u8_truncate(&s, 10);
    printf("jsstr8_u8_truncate: %zu\n", jsstr8_get_utf32len(&s));

    jsstr8_u8_slice(&s_slice, &s, 3, 7);
    printf("jsstr8_u8_slice: %zu\n", jsstr8_get_utf32len(&s_slice));

    jsstr8_init_from_str(&s_str, "Hello, World!");
    printf("jsstr8_init_from_str: %zu\n", jsstr8_get_utf32len(&s_str));

    jsstr8_init_from_str(&s_cmp, "Hello, World!");
    printf("jsstr8_u8_cmp: %d\n", jsstr8_u8_cmp(&s_str, &s_cmp));

    uint32_t search_c = L'o';
    size_t start_i = 6;
    printf("jsstr8_u32_indexof: %zd\n", jsstr8_u32_indexof(&s_str, search_c, start_i));

    uint8_t search_c8 = 'o';
    printf("jsstr8_u8_indexof: %zd\n", jsstr8_u8_indexof(&s_str, search_c8, start_i));

    int32_t search_tok8i[] = L"orld";
    uint32_t *search_tok8 = search_tok8i;
    size_t search_tok8_len = sizeof(search_tok8)/sizeof(search_tok8[0]) - 1;
    printf("jsstr8_u32_indextoken: %zd\n",
           jsstr8_u32_indextoken(&s_str, search_tok8, search_tok8_len, start_i));

    uint8_t search_tok8u[] = "orld";
    size_t search_tok8u_len = utf8_strlen(search_tok8u);
    printf("jsstr8_u8_indextoken: %zd\n",
              jsstr8_u8_indextoken(&s_str, search_tok8u, search_tok8u_len, start_i));

    /* character u32 encoding ops */
    u8 = jsstr8_u32s_at(&s_str, 20);
    if (u8 == NULL) {
        printf("jsstr8_u32s_at: NULL\n");
    } else {
        UTF8_DECODE(&u8, u8 + 4, &c_ptr, c_ptr + 1);
        printf("jsstr8_u32s_at: %lc\n", c);
    }

    jsstr8_u32_truncate(&s, 10);
    printf("jsstr8_u32_truncate: %zu\n", jsstr8_get_utf32len(&s));

    jsstr8_u32_slice(&s_slice, &s_str, 3, 7);
    printf("jsstr8_u32_slice: %zu\n", jsstr8_get_utf32len(&s_slice));

    /* test cmp */
    jsstr8_init_from_str(&s_cmp, "Hello, World!");
    printf("jsstr8_u32_cmp: %d\n", jsstr8_u32_cmp(&s_str, &s_cmp));

    jsstr8_t prefix;
    jsstr8_t suffix;
    jsstr8_t sub;
    jsstr8_init_from_str(&prefix, "Hello");
    jsstr8_init_from_str(&suffix, "World!");
    jsstr8_init_from_str(&sub, ", Wo");
    printf("jsstr8_u8_startswith: %d\n", jsstr8_u8_startswith(&s_str, &prefix));
    printf("jsstr8_u8_endswith: %d\n", jsstr8_u8_endswith(&s_str, &suffix));
    printf("jsstr8_u8_includes: %d\n", jsstr8_u8_includes(&s_str, &sub));

    jsstr8_init_from_buf(&s, (char *)buf, len);
    jsstr8_set_from_utf8(&s, (uint8_t *)"abc", 3);
    jsstr8_u8_toupper(&s);
    printf("jsstr8_u8_toupper: %c%c%c\n", s.bytes[0], s.bytes[1], s.bytes[2]);
    jsstr8_u8_tolower(&s);
    printf("jsstr8_u8_tolower: %c%c%c\n", s.bytes[0], s.bytes[1], s.bytes[2]);
    jsstr8_u32_toupper(&s);
    printf("jsstr8_u32_toupper: %c%c%c\n", s.bytes[0], s.bytes[1], s.bytes[2]);
    jsstr8_u32_tolower(&s);
    printf("jsstr8_u32_tolower: %c%c%c\n", s.bytes[0], s.bytes[1], s.bytes[2]);
    jsstr8_init_from_buf(&dest8, (char *)dest8_buf, dest8_len);
    jsstr8_repeat(&dest8, &s, 3);
    printf("jsstr8_repeat len: %zu\n", jsstr8_get_utf8len(&dest8));
}

void test_jsstr32_well_formed() {
    jsstr32_t valid;
    jsstr32_init_from_str(&valid, L"A");
    printf("jsstr32_is_well_formed (valid): %d\n", jsstr32_is_well_formed(&valid));

    uint32_t invalid_buf[2] = {0xD800, 'A'};
    jsstr32_t invalid = {2, 2, invalid_buf};
    printf("jsstr32_is_well_formed (invalid): %d\n", jsstr32_is_well_formed(&invalid));
    jsstr32_to_well_formed(&invalid);
    printf("jsstr32_is_well_formed (after): %d\n", jsstr32_is_well_formed(&invalid));
    printf("jsstr32_to_well_formed char: %04x\n", invalid.codepoints[0]);
}

void test_jsstr16_well_formed() {
    uint16_t valid_buf[1] = {'A'};
    jsstr16_t valid = {1, 1, valid_buf};
    printf("jsstr16_is_well_formed (valid): %d\n", jsstr16_is_well_formed(&valid));

    uint16_t invalid_buf[2] = {0xD800, 'A'};
    jsstr16_t invalid = {2, 2, invalid_buf};
    printf("jsstr16_is_well_formed (invalid): %d\n", jsstr16_is_well_formed(&invalid));
    jsstr16_to_well_formed(&invalid);
    printf("jsstr16_is_well_formed (after): %d\n", jsstr16_is_well_formed(&invalid));
    printf("jsstr16_to_well_formed char: %04x %04x\n", invalid.codeunits[0], invalid.codeunits[1]);
}

void test_jsstr8_well_formed() {
    uint8_t valid_buf[1] = {'A'};
    jsstr8_t valid = {1, 1, valid_buf};
    printf("jsstr8_is_well_formed (valid): %d\n", jsstr8_is_well_formed(&valid));

    uint8_t invalid_buf[3] = {0xE2, 0x82, 'A'};
    jsstr8_t invalid = {3, 3, invalid_buf};
    printf("jsstr8_is_well_formed (invalid): %d\n", jsstr8_is_well_formed(&invalid));
    uint8_t dest_buf[8];
    jsstr8_t dest = {sizeof(dest_buf), 0, dest_buf};
    jsstr8_to_well_formed(&invalid, &dest);
    printf("jsstr8_is_well_formed (after): %d\n", jsstr8_is_well_formed(&dest));
    printf("jsstr8_to_well_formed bytes: ");
    for (size_t i = 0; i < dest.len; i++) {
        printf("%02x ", dest.bytes[i]);
    }
    printf("\n");
}

void test_jsstr32_to_well_formed() {
    uint32_t invalid_buf[2] = {0xD800, 0x110000};
    jsstr32_t invalid = {2, 2, invalid_buf};
    printf("jsstr32_to_well_formed before: %04x %04x\n", invalid.codepoints[0], invalid.codepoints[1]);
    jsstr32_to_well_formed(&invalid);
    printf("jsstr32_to_well_formed after: %04x %04x\n", invalid.codepoints[0], invalid.codepoints[1]);
}

void test_jsstr16_to_well_formed() {
    uint16_t invalid_buf[2] = {0xD800, 'A'};
    jsstr16_t invalid = {2, 2, invalid_buf};
    printf("jsstr16_to_well_formed before: %04x %04x\n", invalid.codeunits[0], invalid.codeunits[1]);
    jsstr16_to_well_formed(&invalid);
    printf("jsstr16_to_well_formed after: %04x %04x\n", invalid.codeunits[0], invalid.codeunits[1]);
}

void test_jsstr8_to_well_formed() {
    uint8_t invalid_buf[2] = {0x80, 'B'};
    jsstr8_t invalid = {2, 2, invalid_buf};
    uint8_t dest_buf[8];
    jsstr8_t dest = {sizeof(dest_buf), 0, dest_buf};
    printf("jsstr8_to_well_formed before: %02x %02x\n", invalid.bytes[0], invalid.bytes[1]);
    jsstr8_to_well_formed(&invalid, &dest);
    printf("jsstr8_to_well_formed after: ");
    for (size_t i = 0; i < dest.len; i++) {
        printf("%02x ", dest.bytes[i]);
    }
    printf("\n");
}


int main() {
    setlocale(LC_ALL, "");
    test_jsstr32_lifecycle();
    test_jsstr16_lifecycle();
    test_jsstr8_lifecycle();
    test_jsstr32_well_formed();
    test_jsstr16_well_formed();
    test_jsstr8_well_formed();
    test_jsstr32_to_well_formed();
    test_jsstr16_to_well_formed();
    test_jsstr8_to_well_formed();
    return 0;
}
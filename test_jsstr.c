#include <stdio.h>
#include <string.h>
#include "jsstr.h"
#include "utf8.h"
#include <locale.h>



void test_jsstr_lifecycle() {
    jsstr32_t s;
    jsstr32_t s_slice;
    char buf[1024];
    size_t len = sizeof (buf);
    size_t rc;

    jsstr32_init_from_buf(&s, buf, len);
    printf("jsstr32_init_from_buf: %zu\n", jsstr32_get_cap(&s));

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    len = sizeof(utf8_str) - 1;
    rc = jsstr32_set_from_utf8(&s, utf8_str, len);
    printf("jsstr32_set_from_utf8: %zu\n", rc);

    printf("jsstr32_get_utf32len: %zu\n", jsstr32_get_utf32len(&s));
    printf("jsstr32_get_utf16len: %zu\n", jsstr32_get_utf16len(&s));
    printf("jsstr32_get_utf8len: %zu\n", jsstr32_get_utf8len(&s));

    uint32_t *c = jsstr32_wstr_codepoint_at(&s, 20);
    printf("jsstr32_wstr_codepoint_at: %lc\n", *c);

    jsstr32_truncate(&s, 19);
    printf("jsstr32_truncate: %zu\n", jsstr32_get_utf32len(&s));

    jsstr32_slice(&s_slice, &s, 7, 10);
    printf("jsstr32_slice: %zu\n", jsstr32_get_utf32len(&s_slice));

    jsstr32_t s2 = jsstr32_from_str(L"Hello, World!");
    printf("jsstr32_from_str: %zu\n", jsstr32_get_utf32len(&s2));

    /* test cmp */
    jsstr32_t s3 = jsstr32_from_str(L"Hello, World!");
    printf("jsstr32_cmp: %d\n", jsstr32_cmp(&s2, &s3));

    /* test indexof */
    uint32_t search_c = L'o';
    size_t start_i = 6;
    printf("jsstr32_indexof: %zd\n", jsstr32_indexof(&s2, search_c, start_i));

    /* test indextoken */
    uint32_t *search_c2 = L"orld";
    size_t search_c_len = wcslen(search_c2);
    printf("jsstr32_indextoken: %zd\n", jsstr32_indextoken(&s2, search_c2, search_c_len, start_i));    
}

void test_jsstr16_lifecycle() {
    jsstr16_t s;
    jsstr16_t s_slice;
    char buf[1024];
    size_t len = sizeof (buf);
    size_t rc;

    jsstr16_init_from_buf(&s, buf, len);
    printf("jsstr16_init_from_buf: %zu\n", jsstr16_get_cap(&s));

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    len = sizeof(utf8_str) - 1;
    rc = jsstr16_set_from_utf8(&s, utf8_str, len);
    printf("jsstr16_set_from_utf8: %zu\n", rc);

    printf("jsstr16_get_utf32len: %zu\n", jsstr16_get_utf32len(&s));
    printf("jsstr16_get_utf16len: %zu\n", jsstr16_get_utf16len(&s));
    printf("jsstr16_get_utf8len: %zu\n", jsstr16_get_utf8len(&s));

    const uint16_t *cu = jsstr16_u16s_codeunit_at(&s, 20);
    uint32_t c;
    uint32_t *c_ptr = &c;
    UTF16_DECODE(&cu, cu + 2, &c_ptr, c_ptr + 1);
    printf("jsstr16_u16s_codeunit_at: %lc\n", c);

    jsstr16_truncate(&s, 19);
    printf("jsstr16_truncate: %zu\n", jsstr16_get_utf32len(&s));

    jsstr16_slice(&s_slice, &s, 7, 10);
    printf("jsstr16_slice: %zu\n", jsstr16_get_utf32len(&s_slice));

    const uint16_t utf16_str[] = {0x0048, 0x0065, 0x006C, 0x006C, 0x006F, 0x002C, 0x0020, 0x0057, 0x006F, 0x0072, 0x006C, 0x0064, 0x0021};
    jsstr16_t s2 = jsstr16_from_str(utf16_str);
    printf("jsstr16_from_str: %zu\n", jsstr16_get_utf32len(&s2));

    /* test cmp */
    jsstr16_t s3 = jsstr16_from_str(utf16_str);
    printf("jsstr16_cmp: %d\n", jsstr16_cmp(&s2, &s3));

    /* test indexof */
    uint32_t search_c = L'o';
    size_t start_i = 6;
    printf("jsstr16_indexof: %zd\n", jsstr16_indexof(&s2, search_c, start_i));

    /* test indextoken */
    uint32_t *search_c2 = L"orld";
    size_t search_c_len = wcslen(search_c2);
    printf("jsstr16_indextoken: %zd\n", jsstr16_indextoken(&s2, search_c2, search_c_len, start_i));
}

void test_jsstr8_lifecycle() {
    jsstr8_t s;
    jsstr8_t s_slice;
    char buf[1024];
    size_t len = sizeof (buf);
    size_t rc;

    jsstr8_init_from_buf(&s, buf, len);
    printf("jsstr8_init_from_buf: %zu\n", jsstr8_get_cap(&s));

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    len = sizeof(utf8_str) - 1;
    rc = jsstr8_set_from_utf8(&s, utf8_str, len);
    printf("jsstr8_set_from_utf8: %zu\n", rc);

    printf("jsstr8_get_utf32len: %zu\n", jsstr8_get_utf32len(&s));
    printf("jsstr8_get_utf16len: %zu\n", jsstr8_get_utf16len(&s));
    printf("jsstr8_get_utf8len: %zu\n", jsstr8_get_utf8len(&s));

    uint8_t *u = jsstr8_u8s_byte_at(&s, 20);
    printf("jsstr8_u8s_byte_at: %s\n", u);

    jsstr8_truncate(&s, 19);
    printf("jsstr8_truncate: %zu\n", jsstr8_get_utf32len(&s));

    jsstr8_slice(&s_slice, &s, 7, 10);
    printf("jsstr8_slice: %zu\n", jsstr8_get_utf32len(&s_slice));

    jsstr8_t s2 = jsstr8_from_str("Hello, World!");
    printf("jsstr8_from_str: %zu\n", jsstr8_get_utf32len(&s2));

    /* test cmp */
    jsstr8_t s3 = jsstr8_from_str("Hello, World!");
    printf("jsstr8_cmp: %d\n", jsstr8_cmp(&s2, &s3));

    /* test indexof */
    char search_c = 'o';
    size_t start_i = 6;
    printf("jsstr8_indexof: %zd\n", jsstr8_indexof(&s2, search_c, start_i));
    
    /* test indextoken */
    uint32_t *search_c2 = L"orld";
    size_t search_c_len = wcslen(search_c2);
    printf("jsstr8_indextoken: %zd\n", jsstr8_indextoken(&s2, search_c2, search_c_len, start_i));
}


int main() {
    setlocale(LC_ALL, "");
    test_jsstr_lifecycle();
    test_jsstr16_lifecycle();
    test_jsstr8_lifecycle();
    return 0;
}
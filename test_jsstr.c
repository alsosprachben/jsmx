#include <stdio.h>
#include <string.h>
#include "jsstr.h"
#include <locale.h>



void test_jsstr_lifecycle() {
    jsstr_t s;
    jsstr_t s_slice;
    char buf[1024];
    size_t len = sizeof (buf);

    jsstr_init_from_buf(&s, buf, len);
    printf("jsstr_init_from_buf: %zu\n", jsstr_get_cap(&s));

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    len = sizeof(utf8_str) - 1;
    jsstr_set_from_utf8(&s, utf8_str, len);
    printf("jsstr_set_from_utf8: %zu\n", jsstr_get_charlen(&s));

    printf("jsstr_get_utf16len: %zu\n", jsstr_get_utf16len(&s));
    printf("jsstr_get_utf8len: %zu\n", jsstr_get_utf8len(&s));

    wchar_t *c = jsstr_get_at(&s, 20);
    printf("jsstr_get_at: %lc\n", *c);

    jsstr_truncate(&s, 19);
    printf("jsstr_truncate: %zu\n", jsstr_get_charlen(&s));

    jsstr_slice(&s_slice, &s, 7, 10);
    printf("jsstr_slice: %zu\n", jsstr_get_charlen(&s_slice));
}

void test_jsstr16_lifecycle() {
    jsstr16_t s;
    jsstr16_t s_slice;
    char buf[1024];
    size_t len = sizeof (buf);

    jsstr16_init_from_buf(&s, buf, len);
    printf("jsstr16_init_from_buf: %zu\n", jsstr16_get_cap(&s));

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    len = sizeof(utf8_str) - 1;
    jsstr16_set_from_utf8(&s, utf8_str, len);
    printf("jsstr16_set_from_utf8: %zu\n", jsstr16_get_charlen(&s));

    printf("jsstr16_get_utf16len: %zu\n", jsstr16_get_utf16len(&s));
    printf("jsstr16_get_utf8len: %zu\n", jsstr16_get_utf8len(&s));

    uint16_t *cu = jsstr16_get_at(&s, 20);
    wchar_t c;
    if (cu[0] >= 0xD800 && cu[0] <= 0xDBFF && cu[1] >= 0xDC00 && cu[1] <= 0xDFFF) {
        /* surrogate pair */
        c = 0x10000 + ((cu[0] - 0xD800) << 10) + (cu[1] - 0xDC00);
        printf("jsstr16_get_at: %lc\n", c);
    } else {
        c = cu[0];
        printf("jsstr16_get_at: %lc\n", c);
    }

    jsstr16_truncate(&s, 19);
    printf("jsstr16_truncate: %zu\n", jsstr16_get_charlen(&s));

    jsstr16_slice(&s_slice, &s, 7, 10);
    printf("jsstr16_slice: %zu\n", jsstr16_get_charlen(&s_slice));
}

void test_jsstr8_lifecycle() {
    jsstr8_t s;
    jsstr8_t s_slice;
    char buf[1024];
    size_t len = sizeof (buf);

    jsstr8_init_from_buf(&s, buf, len);
    printf("jsstr8_init_from_buf: %zu\n", jsstr8_get_cap(&s));

    uint8_t utf8_str[] = "Hello, UTF-8 World! \xF0\x9F\x98\x81"; // Adding a UTF-8 sequence for 😀
    len = sizeof(utf8_str) - 1;
    jsstr8_set_from_utf8(&s, utf8_str, len);
    printf("jsstr8_set_from_utf8: %zu\n", jsstr8_get_charlen(&s));

    printf("jsstr8_get_utf16len: %zu\n", jsstr8_get_utf16len(&s));
    printf("jsstr8_get_utf8len: %zu\n", jsstr8_get_utf8len(&s));

    uint8_t *u = jsstr8_get_at(&s, 20);
    printf("jsstr8_get_at: %s\n", u);

    jsstr8_truncate(&s, 19);
    printf("jsstr8_truncate: %zu\n", jsstr8_get_charlen(&s));

    jsstr8_slice(&s_slice, &s, 7, 10);
    printf("jsstr8_slice: %zu\n", jsstr8_get_charlen(&s_slice));
}


int main() {
    setlocale(LC_ALL, "");
    test_jsstr_lifecycle();
    test_jsstr16_lifecycle();
    test_jsstr8_lifecycle();
    return 0;
}
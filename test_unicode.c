#include <stdio.h>
#include "unicode.h"

static void test_case_conversion() {
    uint32_t a = 'A';
    uint32_t z = 'z';
    printf("tolower A -> %c\n", (char)unicode_tolower(a));
    printf("toupper z -> %c\n", (char)unicode_toupper(z));
    /* sample from mapping: should handle ascii using map*/
    uint32_t p = unicode_tolower('P');
    uint32_t g = unicode_toupper('g');
    printf("P->%c g->%c\n", (char)p, (char)g);
}

int main() {
    test_case_conversion();
    return 0;
}

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

static void test_exclusions() {
    printf("0958 excluded: %d\n", unicode_is_composition_exclusion(0x0958));
    printf("0041 excluded: %d\n", unicode_is_composition_exclusion(0x0041));
}

static void test_qc() {
    printf("NFD_QC 00C0: %d\n", unicode_get_nfd_qc(0x00C0));
    printf("NFC_QC 0341: %d\n", unicode_get_nfc_qc(0x0341));
    printf("NFC_QC 0041: %d\n", unicode_get_nfc_qc(0x0041));
}

static void test_nfkc_maps() {
    uint32_t buf[UNICODE_NFKC_CF_MAX];
    size_t len = unicode_nfkc_cf(0x00A8, buf);
    printf("NFKC_CF 00A8 len %zu first %04X\n", len, buf[0]);
    len = unicode_nfkc_scf(0x00A8, buf);
    printf("NFKC_SCF 00A8 len %zu first %04X\n", len, buf[0]);
}

int main() {
    test_case_conversion();
    test_exclusions();
    test_qc();
    test_nfkc_maps();
    return 0;
}

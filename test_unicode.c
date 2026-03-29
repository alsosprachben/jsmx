#include <assert.h>
#include <errno.h>
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

static void test_normalize_api() {
    uint32_t src[] = {0x0041, 0x030A};
    uint32_t dst[4] = {0};
    uint32_t buf[4] = {0x0041, 0x030A, 0, 0};
    size_t len;

    len = unicode_normalize_into(src, 2, dst, 4);
    assert(len == 1);
    assert(dst[0] == 0x00C5);

    len = unicode_normalize(buf, 2, 4);
    assert(len == 1);
    assert(buf[0] == 0x00C5);
}

static void test_normalize_forms() {
    uint32_t src[] = {0x1E9B, 0x0323};
    uint32_t dst[8] = {0};
    uint32_t buf[8] = {0x1E9B, 0x0323, 0, 0, 0, 0, 0, 0};
    uint32_t hangul_decomp[] = {0x1100, 0x1161};
    uint32_t hangul_comp[4] = {0xAC00, 0, 0, 0};
    size_t len;

    len = unicode_normalize_into_form(src, 2, dst, 8, UNICODE_NORMALIZE_NFC);
    assert(len == 2);
    assert(dst[0] == 0x1E9B);
    assert(dst[1] == 0x0323);

    len = unicode_normalize_into_form(src, 2, dst, 8, UNICODE_NORMALIZE_NFD);
    assert(len == 3);
    assert(dst[0] == 0x017F);
    assert(dst[1] == 0x0323);
    assert(dst[2] == 0x0307);

    len = unicode_normalize_into_form(src, 2, dst, 8, UNICODE_NORMALIZE_NFKC);
    assert(len == 1);
    assert(dst[0] == 0x1E69);

    len = unicode_normalize_into_form(src, 2, dst, 8, UNICODE_NORMALIZE_NFKD);
    assert(len == 3);
    assert(dst[0] == 0x0073);
    assert(dst[1] == 0x0323);
    assert(dst[2] == 0x0307);

    len = unicode_normalize_form(buf, 2, 8, UNICODE_NORMALIZE_NFKC);
    assert(len == 1);
    assert(buf[0] == 0x1E69);

    len = unicode_normalize_into_form(hangul_decomp, 2, dst, 8, UNICODE_NORMALIZE_NFC);
    assert(len == 1);
    assert(dst[0] == 0xAC00);

    len = unicode_normalize_form(hangul_comp, 1, 4, UNICODE_NORMALIZE_NFD);
    assert(len == 2);
    assert(hangul_comp[0] == 0x1100);
    assert(hangul_comp[1] == 0x1161);
}

static void test_locale_special_casing() {
    uint32_t out[3] = {0};
    size_t len;

    len = unicode_tolower_full_locale(0x0130, "tr", out);
    assert(len == 1);
    assert(out[0] == 0x0069);

    len = unicode_toupper_full_locale(0x0069, "tr", out);
    assert(len == 1);
    assert(out[0] == 0x0130);

    len = unicode_tolower_full_locale(0x00CC, "lt", out);
    assert(len == 3);
    assert(out[0] == 0x0069);
    assert(out[1] == 0x0307);
    assert(out[2] == 0x0300);

    len = unicode_tolower_full_locale(0x0130, NULL, out);
    assert(len == 2);
    assert(out[0] == 0x0069);
    assert(out[1] == 0x0307);
}

static void test_normalization_form_parse() {
    unicode_normalization_form_t form;

    assert(unicode_normalization_form_parse("NFC", 3, &form) == 0);
    assert(form == UNICODE_NORMALIZE_NFC);
    assert(unicode_normalization_form_parse("NFD", 3, &form) == 0);
    assert(form == UNICODE_NORMALIZE_NFD);
    assert(unicode_normalization_form_parse("NFKC", 4, &form) == 0);
    assert(form == UNICODE_NORMALIZE_NFKC);
    assert(unicode_normalization_form_parse("NFKD", 4, &form) == 0);
    assert(form == UNICODE_NORMALIZE_NFKD);

    assert(unicode_normalization_form_name(UNICODE_NORMALIZE_NFC) != NULL);
    assert(unicode_normalization_form_name(UNICODE_NORMALIZE_NFD) != NULL);
    assert(unicode_normalization_form_name(UNICODE_NORMALIZE_NFKC) != NULL);
    assert(unicode_normalization_form_name(UNICODE_NORMALIZE_NFKD) != NULL);

    errno = 0;
    assert(unicode_normalization_form_parse("bar", 3, &form) == -1);
    assert(errno == EINVAL);
    errno = 0;
    assert(unicode_normalization_form_parse("NFC1", 4, &form) == -1);
    assert(errno == EINVAL);
    errno = 0;
    assert(unicode_normalization_form_parse("null", 4, &form) == -1);
    assert(errno == EINVAL);
}

int main() {
    test_case_conversion();
    test_exclusions();
    test_qc();
    test_nfkc_maps();
    test_normalize_api();
    test_normalize_forms();
    test_locale_special_casing();
    test_normalization_form_parse();
    return 0;
}

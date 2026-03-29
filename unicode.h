#ifndef UNICODE_H
#define UNICODE_H
#include <stdint.h>
#include <stddef.h>

typedef enum unicode_normalization_form_e {
    UNICODE_NORMALIZE_NFC = 0,
    UNICODE_NORMALIZE_NFD = 1,
    UNICODE_NORMALIZE_NFKC = 2,
    UNICODE_NORMALIZE_NFKD = 3,
} unicode_normalization_form_t;

int unicode_normalization_form_parse(const char *form, size_t len,
        unicode_normalization_form_t *out);
const char *unicode_normalization_form_name(unicode_normalization_form_t form);

uint32_t unicode_tolower(uint32_t cp);
uint32_t unicode_toupper(uint32_t cp);
size_t unicode_tolower_full(uint32_t cp, uint32_t out[3]);
size_t unicode_toupper_full(uint32_t cp, uint32_t out[3]);
size_t unicode_tolower_full_locale(uint32_t cp, const char *locale, uint32_t out[3]);
size_t unicode_toupper_full_locale(uint32_t cp, const char *locale, uint32_t out[3]);
int unicode_combining_class(uint32_t cp);
int unicode_collation_lookup(uint32_t cp, uint16_t *primary, uint16_t *secondary, uint16_t *tertiary);

#define UNICODE_NFKC_CF_MAX 18
#define UNICODE_NFKC_SCF_MAX 18
#define UNICODE_QC_YES 2

int unicode_is_composition_exclusion(uint32_t cp);
int unicode_get_nfd_qc(uint32_t cp);
int unicode_get_nfc_qc(uint32_t cp);
int unicode_get_nfkd_qc(uint32_t cp);
int unicode_get_nfkc_qc(uint32_t cp);
size_t unicode_nfkc_cf(uint32_t cp, uint32_t out[UNICODE_NFKC_CF_MAX]);
size_t unicode_nfkc_scf(uint32_t cp, uint32_t out[UNICODE_NFKC_SCF_MAX]);
int unicode_changes_when_nfkc_casefolded(uint32_t cp);

size_t unicode_normalize_form_decompose_len_codepoint(uint32_t cp,
        unicode_normalization_form_t form);
int unicode_normalize_form_workspace_len(const uint32_t *src, size_t len,
        unicode_normalization_form_t form, size_t *workspace_cap_ptr);
int unicode_normalize_form_needed(const uint32_t *src, size_t len,
        unicode_normalization_form_t form, uint32_t *workspace,
        size_t workspace_cap, size_t *needed_len_ptr);

size_t unicode_normalize_into_form(const uint32_t *src, size_t len, uint32_t *dst,
        size_t cap, unicode_normalization_form_t form);
size_t unicode_normalize_form(uint32_t *buf, size_t len, size_t cap,
        unicode_normalization_form_t form);
size_t unicode_normalize_into(const uint32_t *src, size_t len, uint32_t *dst, size_t cap);
size_t unicode_normalize(uint32_t *buf, size_t len, size_t cap);

#endif /* UNICODE_H */

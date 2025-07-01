#ifndef UNICODE_H
#define UNICODE_H
#include <stdint.h>
#include <stddef.h>

uint32_t unicode_tolower(uint32_t cp);
uint32_t unicode_toupper(uint32_t cp);
size_t unicode_tolower_full(uint32_t cp, uint32_t out[3]);
size_t unicode_toupper_full(uint32_t cp, uint32_t out[3]);
size_t unicode_tolower_full_locale(uint32_t cp, const char *locale, uint32_t out[3]);
size_t unicode_toupper_full_locale(uint32_t cp, const char *locale, uint32_t out[3]);
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

#endif /* UNICODE_H */

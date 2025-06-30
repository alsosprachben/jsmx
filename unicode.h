#ifndef UNICODE_H
#define UNICODE_H
#include <stdint.h>
#include <stddef.h>

uint32_t unicode_tolower(uint32_t cp);
uint32_t unicode_toupper(uint32_t cp);
size_t unicode_tolower_full(uint32_t cp, uint32_t out[3]);
size_t unicode_toupper_full(uint32_t cp, uint32_t out[3]);
int unicode_collation_lookup(uint32_t cp, uint16_t *primary, uint16_t *secondary, uint16_t *tertiary);

#endif /* UNICODE_H */

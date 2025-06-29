#ifndef UNICODE_H
#define UNICODE_H
#include <stdint.h>

uint32_t unicode_tolower(uint32_t cp);
uint32_t unicode_toupper(uint32_t cp);
int unicode_collation_lookup(uint32_t cp, uint16_t *primary, uint16_t *secondary, uint16_t *tertiary);

#endif /* UNICODE_H */

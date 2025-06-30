#ifndef UNICODE_COLLATION_H
#define UNICODE_COLLATION_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t code; uint16_t primary; uint16_t secondary; uint16_t tertiary; } unicode_collation_t;
static const unicode_collation_t unicode_collation_db[] = { };
static const size_t unicode_collation_db_len = 0;
static const size_t unicode_collation_skip[] = { 0 };
static const size_t unicode_collation_skip_len = 0;
#endif /* UNICODE_COLLATION_H */

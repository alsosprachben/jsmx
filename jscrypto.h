#ifndef JSCRYPTO_H
#define JSCRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"

int jscrypto_random_bytes(uint8_t *buf, size_t len);
int jscrypto_random_uuid(uint8_t *buf, size_t cap, size_t *len_ptr);

#endif

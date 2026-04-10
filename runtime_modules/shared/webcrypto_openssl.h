#ifndef RUNTIME_MODULES_SHARED_WEBCRYPTO_OPENSSL_H
#define RUNTIME_MODULES_SHARED_WEBCRYPTO_OPENSSL_H

#include <stddef.h>
#include <stdint.h>

#include "jscrypto.h"

static inline int
runtime_webcrypto_random_bytes(uint8_t *buf, size_t len)
{
	return jscrypto_random_bytes(buf, len);
}

static inline int
runtime_webcrypto_random_uuid(uint8_t *buf, size_t cap, size_t *len_ptr)
{
	return jscrypto_random_uuid(buf, cap, len_ptr);
}

#endif

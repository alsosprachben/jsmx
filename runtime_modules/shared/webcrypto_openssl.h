#ifndef RUNTIME_MODULES_SHARED_WEBCRYPTO_OPENSSL_H
#define RUNTIME_MODULES_SHARED_WEBCRYPTO_OPENSSL_H

#include <stddef.h>
#include <stdint.h>

#include "jscrypto.h"

typedef jscrypto_digest_algorithm_t runtime_webcrypto_digest_algorithm_t;

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

static inline int
runtime_webcrypto_digest_algorithm_parse(const uint8_t *name, size_t len,
		runtime_webcrypto_digest_algorithm_t *algorithm_ptr)
{
	return jscrypto_digest_algorithm_parse(name, len, algorithm_ptr);
}

static inline int
runtime_webcrypto_digest_length(runtime_webcrypto_digest_algorithm_t algorithm,
		size_t *len_ptr)
{
	return jscrypto_digest_length(algorithm, len_ptr);
}

static inline int
runtime_webcrypto_digest_block_size_bits(
		runtime_webcrypto_digest_algorithm_t algorithm, size_t *len_ptr)
{
	return jscrypto_digest_block_size_bits(algorithm, len_ptr);
}

static inline int
runtime_webcrypto_digest(runtime_webcrypto_digest_algorithm_t algorithm,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr)
{
	return jscrypto_digest(algorithm, input, input_len, output, cap, len_ptr);
}

static inline int
runtime_webcrypto_hmac(runtime_webcrypto_digest_algorithm_t algorithm,
		const uint8_t *key, size_t key_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	return jscrypto_hmac(algorithm, key, key_len, input, input_len, output,
			cap, len_ptr);
}

static inline int
runtime_webcrypto_hmac_verify(runtime_webcrypto_digest_algorithm_t algorithm,
		const uint8_t *key, size_t key_len, const uint8_t *input,
		size_t input_len, const uint8_t *signature, size_t signature_len,
		int *matches_ptr)
{
	return jscrypto_hmac_verify(algorithm, key, key_len, input, input_len,
			signature, signature_len, matches_ptr);
}

#endif

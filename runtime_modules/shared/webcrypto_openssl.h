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

static inline int
runtime_webcrypto_pbkdf2(runtime_webcrypto_digest_algorithm_t algorithm,
		const uint8_t *password, size_t password_len, const uint8_t *salt,
		size_t salt_len, uint32_t iterations, uint8_t *output,
		size_t output_len)
{
	return jscrypto_pbkdf2(algorithm, password, password_len, salt, salt_len,
			iterations, output, output_len);
}

static inline int
runtime_webcrypto_hkdf(runtime_webcrypto_digest_algorithm_t algorithm,
		const uint8_t *key, size_t key_len, const uint8_t *salt,
		size_t salt_len, const uint8_t *info, size_t info_len,
		uint8_t *output, size_t output_len)
{
	return jscrypto_hkdf(algorithm, key, key_len, salt, salt_len, info,
			info_len, output, output_len);
}

static inline int
runtime_webcrypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr)
{
	return jscrypto_aes_gcm_encrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, input, input_len, output, cap, len_ptr);
}

static inline int
runtime_webcrypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr)
{
	return jscrypto_aes_gcm_decrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, input, input_len, output, cap, len_ptr);
}

static inline int
runtime_webcrypto_aes_ctr_crypt(const uint8_t *key, size_t key_len,
		const uint8_t *counter, size_t counter_len, uint32_t length_bits,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr)
{
	return jscrypto_aes_ctr_crypt(key, key_len, counter, counter_len,
			length_bits, input, input_len, output, cap, len_ptr);
}

static inline int
runtime_webcrypto_aes_cbc_encrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	return jscrypto_aes_cbc_encrypt(key, key_len, iv, iv_len, input,
			input_len, output, cap, len_ptr);
}

static inline int
runtime_webcrypto_aes_cbc_decrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	return jscrypto_aes_cbc_decrypt(key, key_len, iv, iv_len, input,
			input_len, output, cap, len_ptr);
}

static inline int
runtime_webcrypto_aes_kw_wrap(const uint8_t *key, size_t key_len,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr)
{
	return jscrypto_aes_kw_wrap(key, key_len, input, input_len, output, cap,
			len_ptr);
}

static inline int
runtime_webcrypto_aes_kw_unwrap(const uint8_t *key, size_t key_len,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr)
{
	return jscrypto_aes_kw_unwrap(key, key_len, input, input_len, output, cap,
			len_ptr);
}

#endif

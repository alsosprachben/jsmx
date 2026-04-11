#ifndef JSCRYPTO_H
#define JSCRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"

typedef enum jscrypto_digest_algorithm_e {
	JSCRYPTO_DIGEST_SHA1 = 0,
	JSCRYPTO_DIGEST_SHA256 = 1,
	JSCRYPTO_DIGEST_SHA384 = 2,
	JSCRYPTO_DIGEST_SHA512 = 3
} jscrypto_digest_algorithm_t;

int jscrypto_random_bytes(uint8_t *buf, size_t len);
int jscrypto_random_uuid(uint8_t *buf, size_t cap, size_t *len_ptr);
int jscrypto_digest_algorithm_parse(const uint8_t *name, size_t len,
		jscrypto_digest_algorithm_t *algorithm_ptr);
int jscrypto_digest_length(jscrypto_digest_algorithm_t algorithm,
		size_t *len_ptr);
int jscrypto_digest_block_size_bits(jscrypto_digest_algorithm_t algorithm,
		size_t *len_ptr);
int jscrypto_digest(jscrypto_digest_algorithm_t algorithm, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_hmac(jscrypto_digest_algorithm_t algorithm, const uint8_t *key,
		size_t key_len, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_hmac_verify(jscrypto_digest_algorithm_t algorithm,
		const uint8_t *key, size_t key_len, const uint8_t *input,
		size_t input_len, const uint8_t *signature, size_t signature_len,
		int *matches_ptr);
int jscrypto_pbkdf2(jscrypto_digest_algorithm_t algorithm,
		const uint8_t *password, size_t password_len, const uint8_t *salt,
		size_t salt_len, uint32_t iterations, uint8_t *output,
		size_t output_len);
int jscrypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr);

#endif

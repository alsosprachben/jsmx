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
int jscrypto_hkdf(jscrypto_digest_algorithm_t algorithm,
		const uint8_t *key, size_t key_len, const uint8_t *salt,
		size_t salt_len, const uint8_t *info, size_t info_len,
		uint8_t *output, size_t output_len);
int jscrypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_aes_ctr_crypt(const uint8_t *key, size_t key_len,
		const uint8_t *counter, size_t counter_len, uint32_t length_bits,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr);
int jscrypto_aes_cbc_encrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_aes_cbc_decrypt(const uint8_t *key, size_t key_len,
		const uint8_t *iv, size_t iv_len, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr);
int jscrypto_aes_kw_wrap(const uint8_t *key, size_t key_len,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr);
int jscrypto_aes_kw_unwrap(const uint8_t *key, size_t key_len,
		const uint8_t *input, size_t input_len, uint8_t *output, size_t cap,
		size_t *len_ptr);

int jscrypto_ecdsa_p256_generate(uint8_t private_out[32],
		uint8_t public_out_xy[64]);
int jscrypto_ecdsa_p256_public_from_private(const uint8_t private_in[32],
		uint8_t public_out_xy[64]);
int jscrypto_ecdsa_p256_sign(const uint8_t private_in[32],
		jscrypto_digest_algorithm_t hash, const uint8_t *data, size_t data_len,
		uint8_t signature_out[64]);
int jscrypto_ecdsa_p256_verify(const uint8_t public_in_xy[64],
		jscrypto_digest_algorithm_t hash, const uint8_t *data, size_t data_len,
		const uint8_t signature_in[64], int *matches_out);

int jscrypto_rsa_pkcs1_v1_5_generate(uint32_t modulus_bits,
		uint8_t *private_der_out, size_t private_cap,
		size_t *private_len_out);
int jscrypto_rsa_public_from_private(const uint8_t *private_der,
		size_t private_len, uint8_t *public_der_out, size_t public_cap,
		size_t *public_len_out);
int jscrypto_rsa_modulus_bits(const uint8_t *der, size_t der_len,
		int is_private, uint32_t *bits_out);
int jscrypto_rsa_pkcs1_v1_5_sign(const uint8_t *private_der,
		size_t private_len, jscrypto_digest_algorithm_t hash,
		const uint8_t *data, size_t data_len,
		uint8_t *signature_out, size_t sig_cap, size_t *sig_len_out);
int jscrypto_rsa_pkcs1_v1_5_verify(const uint8_t *public_der,
		size_t public_len, jscrypto_digest_algorithm_t hash,
		const uint8_t *data, size_t data_len,
		const uint8_t *signature, size_t signature_len, int *matches_out);
int jscrypto_rsa_public_jwk_components(const uint8_t *public_der,
		size_t public_len,
		uint8_t *n_out, size_t n_cap, size_t *n_len,
		uint8_t *e_out, size_t e_cap, size_t *e_len);
int jscrypto_rsa_private_jwk_components(const uint8_t *private_der,
		size_t private_len,
		uint8_t *n_out, size_t n_cap, size_t *n_len,
		uint8_t *e_out, size_t e_cap, size_t *e_len,
		uint8_t *d_out, size_t d_cap, size_t *d_len,
		uint8_t *p_out, size_t p_cap, size_t *p_len,
		uint8_t *q_out, size_t q_cap, size_t *q_len,
		uint8_t *dp_out, size_t dp_cap, size_t *dp_len,
		uint8_t *dq_out, size_t dq_cap, size_t *dq_len,
		uint8_t *qi_out, size_t qi_cap, size_t *qi_len);
int jscrypto_rsa_public_from_jwk_components(
		const uint8_t *n, size_t n_len, const uint8_t *e, size_t e_len,
		uint8_t *public_der_out, size_t cap, size_t *len_out);
int jscrypto_rsa_private_from_jwk_components(
		const uint8_t *n, size_t n_len, const uint8_t *e, size_t e_len,
		const uint8_t *d, size_t d_len,
		const uint8_t *p, size_t p_len, const uint8_t *q, size_t q_len,
		const uint8_t *dp, size_t dp_len, const uint8_t *dq, size_t dq_len,
		const uint8_t *qi, size_t qi_len,
		uint8_t *private_der_out, size_t cap, size_t *len_out);

int jscrypto_rsa_pss_sign(const uint8_t *private_der, size_t private_len,
		jscrypto_digest_algorithm_t hash, uint32_t salt_length_bytes,
		const uint8_t *data, size_t data_len,
		uint8_t *signature_out, size_t sig_cap, size_t *sig_len_out);
int jscrypto_rsa_pss_verify(const uint8_t *public_der, size_t public_len,
		jscrypto_digest_algorithm_t hash, uint32_t salt_length_bytes,
		const uint8_t *data, size_t data_len,
		const uint8_t *signature, size_t signature_len, int *matches_out);

int jscrypto_ecdsa_p256_spki_to_public(const uint8_t *spki_der,
		size_t spki_len, uint8_t xy_out[64]);
int jscrypto_ecdsa_p256_public_to_spki(const uint8_t xy[64],
		uint8_t *der_out, size_t der_cap, size_t *der_len_out);
int jscrypto_ecdsa_p256_pkcs8_to_private(const uint8_t *pkcs8_der,
		size_t pkcs8_len, uint8_t scalar_out[32]);
int jscrypto_ecdsa_p256_private_to_pkcs8(const uint8_t scalar[32],
		uint8_t *der_out, size_t der_cap, size_t *der_len_out);

int jscrypto_rsa_spki_to_public_pkcs1(const uint8_t *spki_der, size_t spki_len,
		uint8_t *pkcs1_out, size_t pkcs1_cap, size_t *pkcs1_len_out);
int jscrypto_rsa_public_pkcs1_to_spki(const uint8_t *pkcs1, size_t pkcs1_len,
		uint8_t *spki_out, size_t spki_cap, size_t *spki_len_out);
int jscrypto_rsa_pkcs8_to_private_pkcs1(const uint8_t *pkcs8_der,
		size_t pkcs8_len, uint8_t *pkcs1_out, size_t pkcs1_cap,
		size_t *pkcs1_len_out);
int jscrypto_rsa_private_pkcs1_to_pkcs8(const uint8_t *pkcs1,
		size_t pkcs1_len, uint8_t *pkcs8_out, size_t pkcs8_cap,
		size_t *pkcs8_len_out);

#endif

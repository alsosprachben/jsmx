#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jscrypto.h"

static int
is_hex_lower(uint8_t ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

static void
assert_digest_vector(jscrypto_digest_algorithm_t algorithm,
		const uint8_t *expected, size_t expected_len)
{
	uint8_t output[64];
	size_t len = 0;

	assert(jscrypto_digest_length(algorithm, &len) == 0);
	assert(len == expected_len);
	assert(jscrypto_digest(algorithm, (const uint8_t *)"abc", 3, NULL, 0,
			&len) == 0);
	assert(len == expected_len);
	assert(jscrypto_digest(algorithm, (const uint8_t *)"abc", 3, output,
			sizeof(output), NULL) == 0);
	assert(memcmp(output, expected, expected_len) == 0);
}

static void
assert_hmac_vector(jscrypto_digest_algorithm_t algorithm, const uint8_t *key,
		size_t key_len, const uint8_t *input, size_t input_len,
		const uint8_t *expected, size_t expected_len)
{
	uint8_t output[64];
	size_t len = 0;
	int matches = 0;

	assert(jscrypto_hmac(algorithm, key, key_len, input, input_len, NULL, 0,
			&len) == 0);
	assert(len == expected_len);
	assert(jscrypto_hmac(algorithm, key, key_len, input, input_len, output,
			sizeof(output), NULL) == 0);
	assert(memcmp(output, expected, expected_len) == 0);
	assert(jscrypto_hmac_verify(algorithm, key, key_len, input, input_len,
			expected, expected_len, &matches) == 0);
	assert(matches == 1);
	output[0] ^= 0x01u;
	assert(jscrypto_hmac_verify(algorithm, key, key_len, input, input_len,
			output, expected_len, &matches) == 0);
	assert(matches == 0);
}

static void
assert_aes_gcm_vector(const uint8_t *key, size_t key_len, const uint8_t *iv,
		size_t iv_len, const uint8_t *aad, size_t aad_len,
		uint32_t tag_bits, const uint8_t *input, size_t input_len,
		const uint8_t *expected, size_t expected_len)
{
	uint8_t output[128];
	uint8_t decrypted[128];
	size_t len = 0;

	assert(jscrypto_aes_gcm_encrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, input, input_len, NULL, 0, &len) == 0);
	assert(len == expected_len);
	assert(jscrypto_aes_gcm_encrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, input, input_len, output, sizeof(output), NULL) == 0);
	assert(memcmp(output, expected, expected_len) == 0);
	assert(jscrypto_aes_gcm_decrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, output, expected_len, NULL, 0, &len) == 0);
	assert(len == input_len);
	assert(jscrypto_aes_gcm_decrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, output, expected_len, decrypted, sizeof(decrypted),
			NULL) == 0);
	assert(memcmp(decrypted, input, input_len) == 0);
	output[expected_len - 1] ^= 0x01u;
	assert(jscrypto_aes_gcm_decrypt(key, key_len, iv, iv_len, aad, aad_len,
			tag_bits, output, expected_len, decrypted, sizeof(decrypted),
			NULL) == -1);
	assert(errno == EIO);
}

static void
assert_pbkdf2_vector(jscrypto_digest_algorithm_t algorithm,
		const uint8_t *password, size_t password_len, const uint8_t *salt,
		size_t salt_len, uint32_t iterations, const uint8_t *expected,
		size_t expected_len)
{
	uint8_t output[128];

	assert(expected_len <= sizeof(output));
	assert(jscrypto_pbkdf2(algorithm, password, password_len, salt, salt_len,
			iterations, output, expected_len) == 0);
	assert(memcmp(output, expected, expected_len) == 0);
}

static void
assert_hkdf_vector(jscrypto_digest_algorithm_t algorithm, const uint8_t *key,
		size_t key_len, const uint8_t *salt, size_t salt_len,
		const uint8_t *info, size_t info_len, const uint8_t *expected,
		size_t expected_len)
{
	uint8_t output[128];

	assert(expected_len <= sizeof(output));
	assert(jscrypto_hkdf(algorithm, key, key_len, salt, salt_len, info,
			info_len, output, expected_len) == 0);
	assert(memcmp(output, expected, expected_len) == 0);
}

static void
assert_aes_ctr_vector(const uint8_t *key, size_t key_len,
		const uint8_t *counter, const uint8_t *plaintext, size_t plaintext_len,
		const uint8_t *expected_ciphertext)
{
	uint8_t output[64];
	uint8_t roundtrip[64];
	size_t len = 0;

	assert(plaintext_len <= sizeof(output));
	assert(jscrypto_aes_ctr_crypt(key, key_len, counter, 16, 32, plaintext,
			plaintext_len, NULL, 0, &len) == 0);
	assert(len == plaintext_len);
	assert(jscrypto_aes_ctr_crypt(key, key_len, counter, 16, 32, plaintext,
			plaintext_len, output, sizeof(output), NULL) == 0);
	assert(memcmp(output, expected_ciphertext, plaintext_len) == 0);
	assert(jscrypto_aes_ctr_crypt(key, key_len, counter, 16, 32, output,
			plaintext_len, roundtrip, sizeof(roundtrip), NULL) == 0);
	assert(memcmp(roundtrip, plaintext, plaintext_len) == 0);
}

static void
assert_aes_cbc_vector(const uint8_t *key, size_t key_len, const uint8_t *iv,
		const uint8_t *plaintext, size_t plaintext_len,
		const uint8_t *expected_ciphertext, size_t expected_ciphertext_len)
{
	uint8_t output[128];
	uint8_t roundtrip[128];
	size_t len = 0;

	assert(expected_ciphertext_len <= sizeof(output));
	/* Sizing call: NULL output reports the required ciphertext capacity. */
	assert(jscrypto_aes_cbc_encrypt(key, key_len, iv, 16, plaintext,
			plaintext_len, NULL, 0, &len) == 0);
	assert(len == expected_ciphertext_len);
	/* Real encrypt. */
	assert(jscrypto_aes_cbc_encrypt(key, key_len, iv, 16, plaintext,
			plaintext_len, output, sizeof(output), NULL) == 0);
	assert(memcmp(output, expected_ciphertext, expected_ciphertext_len) == 0);
	/* Decrypt round-trip. */
	assert(jscrypto_aes_cbc_decrypt(key, key_len, iv, 16, output,
			expected_ciphertext_len, roundtrip, sizeof(roundtrip), &len) == 0);
	assert(len == plaintext_len);
	assert(memcmp(roundtrip, plaintext, plaintext_len) == 0);
	/* Tampering the last byte of the padding block must fail validation. */
	output[expected_ciphertext_len - 1] ^= 0x01u;
	assert(jscrypto_aes_cbc_decrypt(key, key_len, iv, 16, output,
			expected_ciphertext_len, roundtrip, sizeof(roundtrip), &len) == -1);
	assert(errno == EINVAL);
}

static void
assert_aes_kw_vector(const uint8_t *key, size_t key_len,
		const uint8_t *plaintext, size_t plaintext_len,
		const uint8_t *expected_ciphertext)
{
	uint8_t output[64];
	uint8_t roundtrip[64];
	size_t len = 0;
	size_t expected_len = plaintext_len + 8u;

	assert(expected_len <= sizeof(output));
	/* Sizing call. */
	assert(jscrypto_aes_kw_wrap(key, key_len, plaintext, plaintext_len, NULL,
			0, &len) == 0);
	assert(len == expected_len);
	/* Wrap. */
	assert(jscrypto_aes_kw_wrap(key, key_len, plaintext, plaintext_len, output,
			sizeof(output), NULL) == 0);
	assert(memcmp(output, expected_ciphertext, expected_len) == 0);
	/* Unwrap round-trip. */
	assert(jscrypto_aes_kw_unwrap(key, key_len, output, expected_len, roundtrip,
			sizeof(roundtrip), &len) == 0);
	assert(len == plaintext_len);
	assert(memcmp(roundtrip, plaintext, plaintext_len) == 0);
	/* Tampering the last byte must fail the AIV check. */
	output[expected_len - 1] ^= 0x01u;
	assert(jscrypto_aes_kw_unwrap(key, key_len, output, expected_len, roundtrip,
			sizeof(roundtrip), &len) == -1);
	assert(errno == EINVAL);
}

int
main(void)
{
	static const uint8_t sha1_abc[] = {
		0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,
		0xba, 0x3e, 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c,
		0x9c, 0xd0, 0xd8, 0x9d
	};
	static const uint8_t sha256_abc[] = {
		0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
		0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
		0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
		0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
	};
	static const uint8_t sha384_abc[] = {
		0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
		0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
		0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
		0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
		0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
		0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7
	};
	static const uint8_t sha512_abc[] = {
		0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
		0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
		0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
		0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
		0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
		0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
		0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
		0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f
	};
	static const uint8_t hmac_key[] = {
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b
	};
	static const uint8_t hmac_input[] = {
		'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e'
	};
	static const uint8_t hmac_sha1_hi_there[] = {
		0xb6, 0x17, 0x31, 0x86, 0x55, 0x05, 0x72, 0x64,
		0xe2, 0x8b, 0xc0, 0xb6, 0xfb, 0x37, 0x8c, 0x8e,
		0xf1, 0x46, 0xbe, 0x00
	};
	static const uint8_t hmac_sha256_hi_there[] = {
		0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
		0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
		0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
		0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
	};
	static const uint8_t hmac_sha384_hi_there[] = {
		0xaf, 0xd0, 0x39, 0x44, 0xd8, 0x48, 0x95, 0x62,
		0x6b, 0x08, 0x25, 0xf4, 0xab, 0x46, 0x90, 0x7f,
		0x15, 0xf9, 0xda, 0xdb, 0xe4, 0x10, 0x1e, 0xc6,
		0x82, 0xaa, 0x03, 0x4c, 0x7c, 0xeb, 0xc5, 0x9c,
		0xfa, 0xea, 0x9e, 0xa9, 0x07, 0x6e, 0xde, 0x7f,
		0x4a, 0xf1, 0x52, 0xe8, 0xb2, 0xfa, 0x9c, 0xb6
	};
	static const uint8_t hmac_sha512_hi_there[] = {
		0x87, 0xaa, 0x7c, 0xde, 0xa5, 0xef, 0x61, 0x9d,
		0x4f, 0xf0, 0xb4, 0x24, 0x1a, 0x1d, 0x6c, 0xb0,
		0x23, 0x79, 0xf4, 0xe2, 0xce, 0x4e, 0xc2, 0x78,
		0x7a, 0xd0, 0xb3, 0x05, 0x45, 0xe1, 0x7c, 0xde,
		0xda, 0xa8, 0x33, 0xb7, 0xd6, 0xb8, 0xa7, 0x02,
		0x03, 0x8b, 0x27, 0x4e, 0xae, 0xa3, 0xf4, 0xe4,
		0xbe, 0x9d, 0x91, 0x4e, 0xeb, 0x61, 0xf1, 0x70,
		0x2e, 0x69, 0x6c, 0x20, 0x3a, 0x12, 0x68, 0x54
	};
	static const uint8_t aes_gcm_zero_key_128[16] = { 0 };
	static const uint8_t aes_gcm_zero_iv_96[12] = { 0 };
	static const uint8_t aes_gcm_empty_ciphertext_tag[] = {
		0x58, 0xe2, 0xfc, 0xce, 0xfa, 0x7e, 0x30, 0x61,
		0x36, 0x7f, 0x1d, 0x57, 0xa4, 0xe7, 0x45, 0x5a
	};
	static const uint8_t aes_gcm_zero_plaintext_16[16] = { 0 };
	static const uint8_t aes_gcm_zero_ciphertext_16_tag[] = {
		0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
		0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78,
		0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
		0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf
	};
	static const uint8_t pbkdf2_password[] = {
		'p', 'a', 's', 's', 'w', 'o', 'r', 'd'
	};
	static const uint8_t pbkdf2_salt[] = {
		's', 'a', 'l', 't'
	};
	static const uint8_t pbkdf2_sha1_password_salt_1[] = {
		0x0c, 0x60, 0xc8, 0x0f, 0x96, 0x1f, 0x0e, 0x71,
		0xf3, 0xa9, 0xb5, 0x24, 0xaf, 0x60, 0x12, 0x06,
		0x2f, 0xe0, 0x37, 0xa6
	};
	static const uint8_t pbkdf2_sha256_password_salt_2[] = {
		0xae, 0x4d, 0x0c, 0x95, 0xaf, 0x6b, 0x46, 0xd3,
		0x2d, 0x0a, 0xdf, 0xf9, 0x28, 0xf0, 0x6d, 0xd0,
		0x2a, 0x30, 0x3f, 0x8e, 0xf3, 0xc2, 0x51, 0xdf,
		0xd6, 0xe2, 0xd8, 0x5a, 0x95, 0x47, 0x4c, 0x43
	};
	static const uint8_t pbkdf2_sha384_password_salt_2[] = {
		0x54, 0xf7, 0x75, 0xc6, 0xd7, 0x90, 0xf2, 0x19,
		0x30, 0x45, 0x91, 0x62, 0xfc, 0x53, 0x5d, 0xbf,
		0x04, 0xa9, 0x39, 0x18, 0x51, 0x27, 0x01, 0x6a,
		0x04, 0x17, 0x6a, 0x07, 0x30, 0xc6, 0xf1, 0xf4,
		0xfb, 0x48, 0x83, 0x2a, 0xd1, 0x26, 0x1b, 0xaa,
		0xdd, 0x2c, 0xed, 0xd5, 0x08, 0x14, 0xb1, 0xc8
	};
	static const uint8_t pbkdf2_sha512_password_salt_2[] = {
		0xe1, 0xd9, 0xc1, 0x6a, 0xa6, 0x81, 0x70, 0x8a,
		0x45, 0xf5, 0xc7, 0xc4, 0xe2, 0x15, 0xce, 0xb6,
		0x6e, 0x01, 0x1a, 0x2e, 0x9f, 0x00, 0x40, 0x71,
		0x3f, 0x18, 0xae, 0xfd, 0xb8, 0x66, 0xd5, 0x3c,
		0xf7, 0x6c, 0xab, 0x28, 0x68, 0xa3, 0x9b, 0x9f,
		0x78, 0x40, 0xed, 0xce, 0x4f, 0xef, 0x5a, 0x82,
		0xbe, 0x67, 0x33, 0x5c, 0x77, 0xa6, 0x06, 0x8e,
		0x04, 0x11, 0x27, 0x54, 0xf2, 0x7c, 0xcf, 0x4e
	};
	static const uint8_t hkdf_ikm_rfc[] = {
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
		0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
	};
	static const uint8_t hkdf_salt_rfc[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
		0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c
	};
	static const uint8_t hkdf_info_rfc[] = {
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4,
		0xf5, 0xf6, 0xf7, 0xf8, 0xf9
	};
	static const uint8_t hkdf_sha256_rfc[] = {
		0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a,
		0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a,
		0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c,
		0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf,
		0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18,
		0x58, 0x65
	};
	static const uint8_t hkdf_zero_8[8] = { 0 };
	static const uint8_t hkdf_sha256_zero_32[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb,
		0x2f, 0x2d, 0xb9, 0x1d, 0xa3, 0xfa, 0xc6, 0xd8,
		0x95, 0x0c, 0xab, 0x2f, 0x2b, 0x2e, 0xe4, 0x51
	};
	static const uint8_t hkdf_sha384_zero_48[] = {
		0x37, 0x28, 0xc6, 0x3f, 0x94, 0x16, 0x20, 0xa8,
		0x54, 0xd3, 0x9e, 0x80, 0xa1, 0x84, 0x36, 0x57,
		0x35, 0xb2, 0xa9, 0x48, 0x79, 0xdf, 0xb5, 0x71,
		0x99, 0x7a, 0x61, 0x1c, 0x59, 0xb1, 0x6b, 0x87,
		0x38, 0xcc, 0x08, 0xa2, 0x66, 0x64, 0x06, 0xf1,
		0x6f, 0x98, 0x5d, 0x1c, 0x81, 0xcd, 0x01, 0x6d
	};
	/* NIST SP800-38A Appendix F.5 AES-CTR test vectors. */
	static const uint8_t ctr_plaintext[64] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
	};
	static const uint8_t ctr_counter_init[16] = {
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};
	static const uint8_t ctr_aes128_key[16] = {
		0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
	};
	static const uint8_t ctr_aes128_ciphertext[64] = {
		0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26,
		0x1b, 0xef, 0x68, 0x64, 0x99, 0x0d, 0xb6, 0xce,
		0x98, 0x06, 0xf6, 0x6b, 0x79, 0x70, 0xfd, 0xff,
		0x86, 0x17, 0x18, 0x7b, 0xb9, 0xff, 0xfd, 0xff,
		0x5a, 0xe4, 0xdf, 0x3e, 0xdb, 0xd5, 0xd3, 0x5e,
		0x5b, 0x4f, 0x09, 0x02, 0x0d, 0xb0, 0x3e, 0xab,
		0x1e, 0x03, 0x1d, 0xda, 0x2f, 0xbe, 0x03, 0xd1,
		0x79, 0x21, 0x70, 0xa0, 0xf3, 0x00, 0x9c, 0xee
	};
	static const uint8_t ctr_aes192_key[24] = {
		0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
		0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
		0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b
	};
	static const uint8_t ctr_aes192_ciphertext[64] = {
		0x1a, 0xbc, 0x93, 0x24, 0x17, 0x52, 0x1c, 0xa2,
		0x4f, 0x2b, 0x04, 0x59, 0xfe, 0x7e, 0x6e, 0x0b,
		0x09, 0x03, 0x39, 0xec, 0x0a, 0xa6, 0xfa, 0xef,
		0xd5, 0xcc, 0xc2, 0xc6, 0xf4, 0xce, 0x8e, 0x94,
		0x1e, 0x36, 0xb2, 0x6b, 0xd1, 0xeb, 0xc6, 0x70,
		0xd1, 0xbd, 0x1d, 0x66, 0x56, 0x20, 0xab, 0xf7,
		0x4f, 0x78, 0xa7, 0xf6, 0xd2, 0x98, 0x09, 0x58,
		0x5a, 0x97, 0xda, 0xec, 0x58, 0xc6, 0xb0, 0x50
	};
	static const uint8_t ctr_aes256_key[32] = {
		0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
		0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
		0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
		0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
	};
	static const uint8_t ctr_aes256_ciphertext[64] = {
		0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5,
		0xb7, 0xa7, 0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28,
		0xf4, 0x43, 0xe3, 0xca, 0x4d, 0x62, 0xb5, 0x9a,
		0xca, 0x84, 0xe9, 0x90, 0xca, 0xca, 0xf5, 0xc5,
		0x2b, 0x09, 0x30, 0xda, 0xa2, 0x3d, 0xe9, 0x4c,
		0xe8, 0x70, 0x17, 0xba, 0x2d, 0x84, 0x98, 0x8d,
		0xdf, 0xc9, 0xc5, 0x8d, 0xb6, 0x7a, 0xad, 0xa6,
		0x13, 0xc2, 0xdd, 0x08, 0x45, 0x79, 0x41, 0xa6
	};
	/*
	 * AES-CBC: NIST SP800-38A Appendix F.2 plaintext + IV; expected
	 * ciphertexts are the F.2.1/F.2.3/F.2.5 64-byte vectors followed by
	 * the WebCrypto-mandated PKCS#7 trailing padding block. Computed
	 * offline with `openssl enc -aes-{128,192,256}-cbc` against the
	 * NIST plaintext. The first 64 bytes of each vector match the NIST
	 * unpadded reference exactly.
	 */
	static const uint8_t cbc_iv[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	static const uint8_t cbc_aes128_key[16] = {
		0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
	};
	static const uint8_t cbc_aes128_ciphertext[80] = {
		0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46,
		0xce, 0xe9, 0x8e, 0x9b, 0x12, 0xe9, 0x19, 0x7d,
		0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee,
		0x95, 0xdb, 0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2,
		0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74, 0x3b,
		0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16,
		0x3f, 0xf1, 0xca, 0xa1, 0x68, 0x1f, 0xac, 0x09,
		0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7,
		0x8c, 0xb8, 0x28, 0x07, 0x23, 0x0e, 0x13, 0x21,
		0xd3, 0xfa, 0xe0, 0x0d, 0x18, 0xcc, 0x20, 0x12
	};
	static const uint8_t cbc_aes192_key[24] = {
		0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52,
		0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
		0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b
	};
	static const uint8_t cbc_aes192_ciphertext[80] = {
		0x4f, 0x02, 0x1d, 0xb2, 0x43, 0xbc, 0x63, 0x3d,
		0x71, 0x78, 0x18, 0x3a, 0x9f, 0xa0, 0x71, 0xe8,
		0xb4, 0xd9, 0xad, 0xa9, 0xad, 0x7d, 0xed, 0xf4,
		0xe5, 0xe7, 0x38, 0x76, 0x3f, 0x69, 0x14, 0x5a,
		0x57, 0x1b, 0x24, 0x20, 0x12, 0xfb, 0x7a, 0xe0,
		0x7f, 0xa9, 0xba, 0xac, 0x3d, 0xf1, 0x02, 0xe0,
		0x08, 0xb0, 0xe2, 0x79, 0x88, 0x59, 0x88, 0x81,
		0xd9, 0x20, 0xa9, 0xe6, 0x4f, 0x56, 0x15, 0xcd,
		0x61, 0x2c, 0xcd, 0x79, 0x22, 0x4b, 0x35, 0x09,
		0x35, 0xd4, 0x5d, 0xd6, 0xa9, 0x8f, 0x81, 0x76
	};
	static const uint8_t cbc_aes256_key[32] = {
		0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
		0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
		0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
		0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
	};
	static const uint8_t cbc_aes256_ciphertext[80] = {
		0xf5, 0x8c, 0x4c, 0x04, 0xd6, 0xe5, 0xf1, 0xba,
		0x77, 0x9e, 0xab, 0xfb, 0x5f, 0x7b, 0xfb, 0xd6,
		0x9c, 0xfc, 0x4e, 0x96, 0x7e, 0xdb, 0x80, 0x8d,
		0x67, 0x9f, 0x77, 0x7b, 0xc6, 0x70, 0x2c, 0x7d,
		0x39, 0xf2, 0x33, 0x69, 0xa9, 0xd9, 0xba, 0xcf,
		0xa5, 0x30, 0xe2, 0x63, 0x04, 0x23, 0x14, 0x61,
		0xb2, 0xeb, 0x05, 0xe2, 0xc3, 0x9b, 0xe9, 0xfc,
		0xda, 0x6c, 0x19, 0x07, 0x8c, 0x6a, 0x9d, 0x1b,
		0x3f, 0x46, 0x17, 0x96, 0xd6, 0xb0, 0xd6, 0xb2,
		0xe0, 0xc2, 0xa7, 0x2b, 0x4d, 0x80, 0xe6, 0x44
	};
	static const uint8_t cbc_empty_aes128_ciphertext[16] = {
		0xc8, 0x4a, 0xf0, 0xb6, 0x13, 0x43, 0x5d, 0x5d,
		0x91, 0x82, 0x80, 0x1a, 0x9b, 0xd9, 0x32, 0x0b
	};
	static const uint8_t cbc_nist_plaintext[64] = {
		0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
		0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
		0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
		0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
		0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
		0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
		0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
		0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10
	};
	/* AES-KW: NIST SP800-38F §5.6 / RFC 3394 example vectors. */
	static const uint8_t kw_aes128_key[16] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	static const uint8_t kw_aes128_plaintext[16] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
	};
	static const uint8_t kw_aes128_ciphertext[24] = {
		0x1f, 0xa6, 0x8b, 0x0a, 0x81, 0x12, 0xb4, 0x47,
		0xae, 0xf3, 0x4b, 0xd8, 0xfb, 0x5a, 0x7b, 0x82,
		0x9d, 0x3e, 0x86, 0x23, 0x71, 0xd2, 0xcf, 0xe5
	};
	static const uint8_t kw_aes192_key[24] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
	};
	static const uint8_t kw_aes192_plaintext[24] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
	};
	static const uint8_t kw_aes192_ciphertext[32] = {
		0x03, 0x1d, 0x33, 0x26, 0x4e, 0x15, 0xd3, 0x32,
		0x68, 0xf2, 0x4e, 0xc2, 0x60, 0x74, 0x3e, 0xdc,
		0xe1, 0xc6, 0xc7, 0xdd, 0xee, 0x72, 0x5a, 0x93,
		0x6b, 0xa8, 0x14, 0x91, 0x5c, 0x67, 0x62, 0xd2
	};
	static const uint8_t kw_aes256_key[32] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
	};
	static const uint8_t kw_aes256_plaintext[32] = {
		0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
		0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	};
	static const uint8_t kw_aes256_ciphertext[40] = {
		0x28, 0xc9, 0xf4, 0x04, 0xc4, 0xb8, 0x10, 0xf4,
		0xcb, 0xcc, 0xb3, 0x5c, 0xfb, 0x87, 0xf8, 0x26,
		0x3f, 0x57, 0x86, 0xe2, 0xd8, 0x0e, 0xd3, 0x26,
		0xcb, 0xc7, 0xf0, 0xe7, 0x1a, 0x99, 0xf4, 0x3b,
		0xfb, 0x98, 0x8b, 0x9b, 0x7a, 0x02, 0xdd, 0x21
	};
	static const uint8_t hkdf_sha512_zero_64[] = {
		0x1b, 0xed, 0xb2, 0xa3, 0xbe, 0xe0, 0xef, 0xff,
		0x14, 0x33, 0x6e, 0x2a, 0x58, 0x56, 0xef, 0xa0,
		0x4d, 0x4f, 0xab, 0x0c, 0x39, 0x27, 0x74, 0x20,
		0x7c, 0x1c, 0x61, 0x40, 0xbc, 0x7d, 0x11, 0x93,
		0x9c, 0x08, 0x24, 0xf3, 0xa7, 0x6e, 0x0f, 0xba,
		0x10, 0x4c, 0xa7, 0xbe, 0xc6, 0x73, 0x1c, 0x62,
		0x68, 0x2a, 0x6d, 0xa1, 0xfd, 0xf6, 0x69, 0x9f,
		0xba, 0x74, 0x9a, 0xfd, 0x21, 0x70, 0x03, 0x9d
	};
 #if JSMX_WITH_CRYPTO
	uint8_t bytes[32];
	uint8_t uuid[36];
	size_t len = 0;
	size_t i;
	int any_changed = 0;
	jscrypto_digest_algorithm_t algorithm;

	memset(bytes, 0, sizeof(bytes));
	assert(jscrypto_random_bytes(bytes, sizeof(bytes)) == 0);
	for (i = 0; i < sizeof(bytes); i++) {
		if (bytes[i] != 0) {
			any_changed = 1;
			break;
		}
	}
	assert(any_changed);

	assert(jscrypto_random_uuid(NULL, 0, &len) == 0);
	assert(len == 36);
	assert(jscrypto_random_uuid(uuid, sizeof(uuid), NULL) == 0);
	for (i = 0; i < sizeof(uuid); i++) {
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			assert(uuid[i] == '-');
		} else {
			assert(is_hex_lower(uuid[i]));
		}
	}
	assert(uuid[14] == '4');
	assert(uuid[19] == '8' || uuid[19] == '9'
			|| uuid[19] == 'a' || uuid[19] == 'b');

	assert(jscrypto_digest_algorithm_parse((const uint8_t *)"SHA-1", 5,
			&algorithm) == 0);
	assert(algorithm == JSCRYPTO_DIGEST_SHA1);
	assert(jscrypto_digest_algorithm_parse((const uint8_t *)"SHA-256", 7,
			&algorithm) == 0);
	assert(algorithm == JSCRYPTO_DIGEST_SHA256);
	assert(jscrypto_digest_algorithm_parse((const uint8_t *)"SHA-384", 7,
			&algorithm) == 0);
	assert(algorithm == JSCRYPTO_DIGEST_SHA384);
	assert(jscrypto_digest_algorithm_parse((const uint8_t *)"SHA-512", 7,
			&algorithm) == 0);
	assert(algorithm == JSCRYPTO_DIGEST_SHA512);
	assert(jscrypto_digest_algorithm_parse((const uint8_t *)"SHA-999", 7,
			&algorithm) == -1);
	assert(errno == ENOTSUP);

	assert_digest_vector(JSCRYPTO_DIGEST_SHA1, sha1_abc, sizeof(sha1_abc));
	assert_digest_vector(JSCRYPTO_DIGEST_SHA256, sha256_abc,
			sizeof(sha256_abc));
	assert_digest_vector(JSCRYPTO_DIGEST_SHA384, sha384_abc,
			sizeof(sha384_abc));
	assert_digest_vector(JSCRYPTO_DIGEST_SHA512, sha512_abc,
			sizeof(sha512_abc));
	assert_hmac_vector(JSCRYPTO_DIGEST_SHA1, hmac_key, sizeof(hmac_key),
			hmac_input, sizeof(hmac_input), hmac_sha1_hi_there,
			sizeof(hmac_sha1_hi_there));
	assert_hmac_vector(JSCRYPTO_DIGEST_SHA256, hmac_key, sizeof(hmac_key),
			hmac_input, sizeof(hmac_input), hmac_sha256_hi_there,
			sizeof(hmac_sha256_hi_there));
	assert_hmac_vector(JSCRYPTO_DIGEST_SHA384, hmac_key, sizeof(hmac_key),
			hmac_input, sizeof(hmac_input), hmac_sha384_hi_there,
			sizeof(hmac_sha384_hi_there));
	assert_hmac_vector(JSCRYPTO_DIGEST_SHA512, hmac_key, sizeof(hmac_key),
			hmac_input, sizeof(hmac_input), hmac_sha512_hi_there,
			sizeof(hmac_sha512_hi_there));
	assert_pbkdf2_vector(JSCRYPTO_DIGEST_SHA1, pbkdf2_password,
			sizeof(pbkdf2_password), pbkdf2_salt, sizeof(pbkdf2_salt), 1,
			pbkdf2_sha1_password_salt_1,
			sizeof(pbkdf2_sha1_password_salt_1));
	assert_pbkdf2_vector(JSCRYPTO_DIGEST_SHA256, pbkdf2_password,
			sizeof(pbkdf2_password), pbkdf2_salt, sizeof(pbkdf2_salt), 2,
			pbkdf2_sha256_password_salt_2,
			sizeof(pbkdf2_sha256_password_salt_2));
	assert_pbkdf2_vector(JSCRYPTO_DIGEST_SHA384, pbkdf2_password,
			sizeof(pbkdf2_password), pbkdf2_salt, sizeof(pbkdf2_salt), 2,
			pbkdf2_sha384_password_salt_2,
			sizeof(pbkdf2_sha384_password_salt_2));
	assert_pbkdf2_vector(JSCRYPTO_DIGEST_SHA512, pbkdf2_password,
			sizeof(pbkdf2_password), pbkdf2_salt, sizeof(pbkdf2_salt), 2,
			pbkdf2_sha512_password_salt_2,
			sizeof(pbkdf2_sha512_password_salt_2));
	assert_hkdf_vector(JSCRYPTO_DIGEST_SHA256, hkdf_ikm_rfc,
			sizeof(hkdf_ikm_rfc), hkdf_salt_rfc, sizeof(hkdf_salt_rfc),
			hkdf_info_rfc, sizeof(hkdf_info_rfc), hkdf_sha256_rfc,
			sizeof(hkdf_sha256_rfc));
	assert_hkdf_vector(JSCRYPTO_DIGEST_SHA256, hkdf_zero_8,
			sizeof(hkdf_zero_8), hkdf_zero_8, sizeof(hkdf_zero_8),
			hkdf_zero_8, sizeof(hkdf_zero_8), hkdf_sha256_zero_32,
			sizeof(hkdf_sha256_zero_32));
	assert_hkdf_vector(JSCRYPTO_DIGEST_SHA384, hkdf_zero_8,
			sizeof(hkdf_zero_8), hkdf_zero_8, sizeof(hkdf_zero_8),
			hkdf_zero_8, sizeof(hkdf_zero_8), hkdf_sha384_zero_48,
			sizeof(hkdf_sha384_zero_48));
	assert_hkdf_vector(JSCRYPTO_DIGEST_SHA512, hkdf_zero_8,
			sizeof(hkdf_zero_8), hkdf_zero_8, sizeof(hkdf_zero_8),
			hkdf_zero_8, sizeof(hkdf_zero_8), hkdf_sha512_zero_64,
			sizeof(hkdf_sha512_zero_64));
	assert_aes_gcm_vector(aes_gcm_zero_key_128, sizeof(aes_gcm_zero_key_128),
			aes_gcm_zero_iv_96, sizeof(aes_gcm_zero_iv_96), NULL, 0, 128, NULL,
			0, aes_gcm_empty_ciphertext_tag,
			sizeof(aes_gcm_empty_ciphertext_tag));
	assert_aes_gcm_vector(aes_gcm_zero_key_128, sizeof(aes_gcm_zero_key_128),
			aes_gcm_zero_iv_96, sizeof(aes_gcm_zero_iv_96), NULL, 0, 128,
			aes_gcm_zero_plaintext_16, sizeof(aes_gcm_zero_plaintext_16),
			aes_gcm_zero_ciphertext_16_tag,
			sizeof(aes_gcm_zero_ciphertext_16_tag));
	{
		uint8_t scratch[64];
		size_t out_len = 0;

		/* AES-GCM rejects empty IV per WebCrypto/SP800-38D. */
		assert(jscrypto_aes_gcm_encrypt(aes_gcm_zero_key_128,
				sizeof(aes_gcm_zero_key_128), aes_gcm_zero_iv_96, 0, NULL, 0,
				128, NULL, 0, scratch, sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);
		assert(jscrypto_aes_gcm_decrypt(aes_gcm_zero_key_128,
				sizeof(aes_gcm_zero_key_128), aes_gcm_zero_iv_96, 0, NULL, 0,
				128, aes_gcm_empty_ciphertext_tag,
				sizeof(aes_gcm_empty_ciphertext_tag), scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);

		/* PBKDF2/HKDF accept zero-length output (returns empty buffer). */
		assert(jscrypto_pbkdf2(JSCRYPTO_DIGEST_SHA256, pbkdf2_password,
				sizeof(pbkdf2_password), pbkdf2_salt, sizeof(pbkdf2_salt), 1,
				scratch, 0) == 0);
		assert(jscrypto_hkdf(JSCRYPTO_DIGEST_SHA256, hkdf_ikm_rfc,
				sizeof(hkdf_ikm_rfc), hkdf_salt_rfc, sizeof(hkdf_salt_rfc),
				hkdf_info_rfc, sizeof(hkdf_info_rfc), scratch, 0) == 0);
	}
	assert_aes_ctr_vector(ctr_aes128_key, sizeof(ctr_aes128_key),
			ctr_counter_init, ctr_plaintext, sizeof(ctr_plaintext),
			ctr_aes128_ciphertext);
	assert_aes_ctr_vector(ctr_aes192_key, sizeof(ctr_aes192_key),
			ctr_counter_init, ctr_plaintext, sizeof(ctr_plaintext),
			ctr_aes192_ciphertext);
	assert_aes_ctr_vector(ctr_aes256_key, sizeof(ctr_aes256_key),
			ctr_counter_init, ctr_plaintext, sizeof(ctr_plaintext),
			ctr_aes256_ciphertext);
	{
		uint8_t scratch[64];
		size_t out_len = 0;

		/* Length parameter must be in [1, 128]. */
		assert(jscrypto_aes_ctr_crypt(ctr_aes128_key, sizeof(ctr_aes128_key),
				ctr_counter_init, 16, 0, ctr_plaintext, 16, scratch,
				sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);
		assert(jscrypto_aes_ctr_crypt(ctr_aes128_key, sizeof(ctr_aes128_key),
				ctr_counter_init, 16, 200, ctr_plaintext, 16, scratch,
				sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);
		/* Counter must be exactly 16 bytes. */
		assert(jscrypto_aes_ctr_crypt(ctr_aes128_key, sizeof(ctr_aes128_key),
				ctr_counter_init, 12, 32, ctr_plaintext, 16, scratch,
				sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);
		/* Key length must be 16/24/32. */
		assert(jscrypto_aes_ctr_crypt(ctr_aes128_key, 8, ctr_counter_init, 16,
				32, ctr_plaintext, 16, scratch, sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);
		/* Block-count overflow: length=1 means at most 2 blocks, 3 fails. */
		assert(jscrypto_aes_ctr_crypt(ctr_aes128_key, sizeof(ctr_aes128_key),
				ctr_counter_init, 16, 1, ctr_plaintext, 33, scratch,
				sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);

		/*
		 * Counter-wrap boundary: with length_bits = 8 and a counter
		 * ending in 0xff, the second keystream block must use the
		 * counter wrapped to ..00 (low byte cleared, all higher bytes
		 * unchanged), not carried into the nonce. Validate by
		 * comparing against single-block length=128 calls (the fast
		 * path, which is verified by the NIST vectors above).
		 */
		{
			uint8_t wrap_counter[16] = {
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
				0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xff
			};
			uint8_t wrapped_counter[16] = {
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
				0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00
			};
			uint8_t carried_counter[16] = {
				0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
				0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x10, 0x00
			};
			uint8_t zero_plaintext[32] = { 0 };
			uint8_t actual[32];
			uint8_t first_block_ref[16];
			uint8_t wrapped_ref[16];
			uint8_t carried_ref[16];

			assert(jscrypto_aes_ctr_crypt(ctr_aes128_key,
					sizeof(ctr_aes128_key), wrap_counter, 16, 8,
					zero_plaintext, sizeof(zero_plaintext), actual,
					sizeof(actual), NULL) == 0);
			assert(jscrypto_aes_ctr_crypt(ctr_aes128_key,
					sizeof(ctr_aes128_key), wrap_counter, 16, 128,
					zero_plaintext, 16, first_block_ref,
					sizeof(first_block_ref), NULL) == 0);
			assert(jscrypto_aes_ctr_crypt(ctr_aes128_key,
					sizeof(ctr_aes128_key), wrapped_counter, 16, 128,
					zero_plaintext, 16, wrapped_ref, sizeof(wrapped_ref),
					NULL) == 0);
			assert(jscrypto_aes_ctr_crypt(ctr_aes128_key,
					sizeof(ctr_aes128_key), carried_counter, 16, 128,
					zero_plaintext, 16, carried_ref, sizeof(carried_ref),
					NULL) == 0);
			assert(memcmp(actual, first_block_ref,
					sizeof(first_block_ref)) == 0);
			assert(memcmp(actual + 16, wrapped_ref,
					sizeof(wrapped_ref)) == 0);
			/* And the buggy carry path must NOT match. */
			assert(memcmp(wrapped_ref, carried_ref,
					sizeof(wrapped_ref)) != 0);
			assert(memcmp(actual + 16, carried_ref,
					sizeof(carried_ref)) != 0);
		}
	}
	assert_aes_cbc_vector(cbc_aes128_key, sizeof(cbc_aes128_key), cbc_iv,
			cbc_nist_plaintext, sizeof(cbc_nist_plaintext),
			cbc_aes128_ciphertext, sizeof(cbc_aes128_ciphertext));
	assert_aes_cbc_vector(cbc_aes192_key, sizeof(cbc_aes192_key), cbc_iv,
			cbc_nist_plaintext, sizeof(cbc_nist_plaintext),
			cbc_aes192_ciphertext, sizeof(cbc_aes192_ciphertext));
	assert_aes_cbc_vector(cbc_aes256_key, sizeof(cbc_aes256_key), cbc_iv,
			cbc_nist_plaintext, sizeof(cbc_nist_plaintext),
			cbc_aes256_ciphertext, sizeof(cbc_aes256_ciphertext));
	{
		uint8_t scratch[80];
		uint8_t roundtrip[80];
		size_t out_len = 0;

		/* AES-CBC negative cases. */
		assert(jscrypto_aes_cbc_encrypt(cbc_aes128_key, 8, cbc_iv, 16,
				cbc_nist_plaintext, sizeof(cbc_nist_plaintext), scratch,
				sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);
		assert(jscrypto_aes_cbc_encrypt(cbc_aes128_key,
				sizeof(cbc_aes128_key), cbc_iv, 12, cbc_nist_plaintext,
				sizeof(cbc_nist_plaintext), scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);
		assert(jscrypto_aes_cbc_decrypt(cbc_aes128_key,
				sizeof(cbc_aes128_key), cbc_iv, 16, cbc_aes128_ciphertext, 13,
				scratch, sizeof(scratch), &out_len) == -1);
		assert(errno == EINVAL);

		/* Empty plaintext: PKCS#7 produces a 16-byte all-padding block. */
		assert(jscrypto_aes_cbc_encrypt(cbc_aes128_key,
				sizeof(cbc_aes128_key), cbc_iv, 16, NULL, 0, NULL, 0,
				&out_len) == 0);
		assert(out_len == 16);
		assert(jscrypto_aes_cbc_encrypt(cbc_aes128_key,
				sizeof(cbc_aes128_key), cbc_iv, 16, NULL, 0, scratch,
				sizeof(scratch), NULL) == 0);
		assert(memcmp(scratch, cbc_empty_aes128_ciphertext,
				sizeof(cbc_empty_aes128_ciphertext)) == 0);
		assert(jscrypto_aes_cbc_decrypt(cbc_aes128_key,
				sizeof(cbc_aes128_key), cbc_iv, 16, scratch, 16, roundtrip,
				sizeof(roundtrip), &out_len) == 0);
		assert(out_len == 0);
	}
	assert_aes_kw_vector(kw_aes128_key, sizeof(kw_aes128_key),
			kw_aes128_plaintext, sizeof(kw_aes128_plaintext),
			kw_aes128_ciphertext);
	assert_aes_kw_vector(kw_aes192_key, sizeof(kw_aes192_key),
			kw_aes192_plaintext, sizeof(kw_aes192_plaintext),
			kw_aes192_ciphertext);
	assert_aes_kw_vector(kw_aes256_key, sizeof(kw_aes256_key),
			kw_aes256_plaintext, sizeof(kw_aes256_plaintext),
			kw_aes256_ciphertext);
	{
		uint8_t scratch[64];
		size_t out_len = 0;

		/* AES-KW negative cases. */
		assert(jscrypto_aes_kw_wrap(kw_aes128_key, 8, kw_aes128_plaintext,
				sizeof(kw_aes128_plaintext), scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);
		/* Input length not a multiple of 8. */
		assert(jscrypto_aes_kw_wrap(kw_aes128_key, sizeof(kw_aes128_key),
				kw_aes128_plaintext, 17, scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);
		/* Input length below the 16-byte minimum. */
		assert(jscrypto_aes_kw_wrap(kw_aes128_key, sizeof(kw_aes128_key),
				kw_aes128_plaintext, 8, scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);
		/* Unwrap input below the 24-byte minimum. */
		assert(jscrypto_aes_kw_unwrap(kw_aes128_key, sizeof(kw_aes128_key),
				kw_aes128_ciphertext, 16, scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);
		/* Unwrap input length not a multiple of 8. */
		assert(jscrypto_aes_kw_unwrap(kw_aes128_key, sizeof(kw_aes128_key),
				kw_aes128_ciphertext, 25, scratch, sizeof(scratch),
				&out_len) == -1);
		assert(errno == EINVAL);
	}

	/* ECDSA P-256: generate, sign, verify round trip + negatives. Also
	 * verifies the DER<->P1363 conversion implicitly (every sign emits
	 * DER internally and converts). */
	{
		uint8_t private_key[32];
		uint8_t public_key[64];
		uint8_t derived_public[64];
		uint8_t signature[64];
		uint8_t message[32];
		size_t i;
		int matches = 0;

		for (i = 0; i < sizeof(message); i++) {
			message[i] = (uint8_t)i;
		}
		assert(jscrypto_ecdsa_p256_generate(private_key, public_key) == 0);
		assert(jscrypto_ecdsa_p256_public_from_private(private_key,
				derived_public) == 0);
		assert(memcmp(public_key, derived_public, 64) == 0);

		assert(jscrypto_ecdsa_p256_sign(private_key, JSCRYPTO_DIGEST_SHA256,
				message, sizeof(message), signature) == 0);
		assert(jscrypto_ecdsa_p256_verify(public_key, JSCRYPTO_DIGEST_SHA256,
				message, sizeof(message), signature, &matches) == 0);
		assert(matches == 1);

		/* Flipped signature byte -> verify resolves false, no error. */
		signature[0] ^= 0x80u;
		assert(jscrypto_ecdsa_p256_verify(public_key, JSCRYPTO_DIGEST_SHA256,
				message, sizeof(message), signature, &matches) == 0);
		assert(matches == 0);

		/* SHA-384 and SHA-512 round-trip. */
		assert(jscrypto_ecdsa_p256_sign(private_key, JSCRYPTO_DIGEST_SHA384,
				message, sizeof(message), signature) == 0);
		assert(jscrypto_ecdsa_p256_verify(public_key, JSCRYPTO_DIGEST_SHA384,
				message, sizeof(message), signature, &matches) == 0);
		assert(matches == 1);
		assert(jscrypto_ecdsa_p256_sign(private_key, JSCRYPTO_DIGEST_SHA512,
				message, sizeof(message), signature) == 0);
		assert(jscrypto_ecdsa_p256_verify(public_key, JSCRYPTO_DIGEST_SHA512,
				message, sizeof(message), signature, &matches) == 0);
		assert(matches == 1);

		/* Message tamper -> verify resolves false. */
		assert(jscrypto_ecdsa_p256_sign(private_key, JSCRYPTO_DIGEST_SHA256,
				message, sizeof(message), signature) == 0);
		message[7] ^= 0x01u;
		assert(jscrypto_ecdsa_p256_verify(public_key, JSCRYPTO_DIGEST_SHA256,
				message, sizeof(message), signature, &matches) == 0);
		assert(matches == 0);
		message[7] ^= 0x01u;

		/* Different key -> verify resolves false. */
		{
			uint8_t other_private[32];
			uint8_t other_public[64];

			assert(jscrypto_ecdsa_p256_generate(other_private,
					other_public) == 0);
			assert(jscrypto_ecdsa_p256_sign(private_key,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
					signature) == 0);
			assert(jscrypto_ecdsa_p256_verify(other_public,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
					signature, &matches) == 0);
			assert(matches == 0);
		}

		/* Signatures are fresh-random across calls (DER<->P1363 doesn't
		 * lose entropy). Sign twice and confirm the signatures differ. */
		{
			uint8_t sig_a[64];
			uint8_t sig_b[64];

			assert(jscrypto_ecdsa_p256_sign(private_key,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
					sig_a) == 0);
			assert(jscrypto_ecdsa_p256_sign(private_key,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
					sig_b) == 0);
			assert(memcmp(sig_a, sig_b, 64) != 0);
			assert(jscrypto_ecdsa_p256_verify(public_key,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
					sig_a, &matches) == 0);
			assert(matches == 1);
			assert(jscrypto_ecdsa_p256_verify(public_key,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
					sig_b, &matches) == 0);
			assert(matches == 1);
		}
	}

	/* RSASSA-PKCS1-v1_5: generate a 2048-bit key, sign, verify, tamper,
	 * round-trip through JWK components, and spot-check larger moduli. */
	{
		uint8_t private_der[4096];
		uint8_t public_der[1024];
		size_t private_len = 0;
		size_t public_len = 0;
		uint8_t message[32];
		uint8_t signature[512];
		size_t sig_len = 0;
		uint32_t bits = 0;
		size_t i;
		int matches = 0;

		for (i = 0; i < sizeof(message); i++) {
			message[i] = (uint8_t)(i * 3u);
		}
		assert(jscrypto_rsa_pkcs1_v1_5_generate(2048, private_der,
				sizeof(private_der), &private_len) == 0);
		assert(private_len > 0 && private_len < sizeof(private_der));
		assert(jscrypto_rsa_public_from_private(private_der, private_len,
				public_der, sizeof(public_der), &public_len) == 0);
		assert(public_len > 0 && public_len < sizeof(public_der));
		assert(jscrypto_rsa_modulus_bits(private_der, private_len, 1,
				&bits) == 0);
		assert(bits == 2048);
		assert(jscrypto_rsa_modulus_bits(public_der, public_len, 0,
				&bits) == 0);
		assert(bits == 2048);

		assert(jscrypto_rsa_pkcs1_v1_5_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(sig_len == 256);
		assert(jscrypto_rsa_pkcs1_v1_5_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);

		/* Flip a signature byte -> verify resolves false. */
		signature[0] ^= 0x80u;
		assert(jscrypto_rsa_pkcs1_v1_5_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 0);
		signature[0] ^= 0x80u;

		/* Tamper the message -> verify resolves false. */
		message[5] ^= 0x01u;
		assert(jscrypto_rsa_pkcs1_v1_5_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 0);
		message[5] ^= 0x01u;

		/* SHA-384 and SHA-512 variants round-trip. */
		assert(jscrypto_rsa_pkcs1_v1_5_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA384, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(sig_len == 256);
		assert(jscrypto_rsa_pkcs1_v1_5_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA384, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);
		assert(jscrypto_rsa_pkcs1_v1_5_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA512, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(sig_len == 256);
		assert(jscrypto_rsa_pkcs1_v1_5_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA512, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);

		/* JWK component extraction + round-trip via
		 * _from_jwk_components then sign/verify. */
		{
			uint8_t n_bytes[512];
			uint8_t e_bytes[8];
			uint8_t d_bytes[512];
			uint8_t p_bytes[256];
			uint8_t q_bytes[256];
			uint8_t dp_bytes[256];
			uint8_t dq_bytes[256];
			uint8_t qi_bytes[256];
			size_t n_len = 0;
			size_t e_len = 0;
			size_t d_len = 0;
			size_t p_len = 0;
			size_t q_len = 0;
			size_t dp_len = 0;
			size_t dq_len = 0;
			size_t qi_len = 0;
			uint8_t rebuilt_private[4096];
			uint8_t rebuilt_public[1024];
			size_t rebuilt_private_len = 0;
			size_t rebuilt_public_len = 0;

			assert(jscrypto_rsa_private_jwk_components(private_der,
					private_len,
					n_bytes, sizeof(n_bytes), &n_len,
					e_bytes, sizeof(e_bytes), &e_len,
					d_bytes, sizeof(d_bytes), &d_len,
					p_bytes, sizeof(p_bytes), &p_len,
					q_bytes, sizeof(q_bytes), &q_len,
					dp_bytes, sizeof(dp_bytes), &dp_len,
					dq_bytes, sizeof(dq_bytes), &dq_len,
					qi_bytes, sizeof(qi_bytes), &qi_len) == 0);
			/* JWK strips leading zeros: 2048-bit modulus -> 256 bytes
			 * unless the top byte happens to be zero (very rare). */
			assert(n_len > 0 && n_len <= 256);
			assert(e_len == 3);
			assert(e_bytes[0] == 0x01u && e_bytes[1] == 0x00u
					&& e_bytes[2] == 0x01u);

			assert(jscrypto_rsa_private_from_jwk_components(
					n_bytes, n_len, e_bytes, e_len, d_bytes, d_len,
					p_bytes, p_len, q_bytes, q_len,
					dp_bytes, dp_len, dq_bytes, dq_len, qi_bytes, qi_len,
					rebuilt_private, sizeof(rebuilt_private),
					&rebuilt_private_len) == 0);
			assert(jscrypto_rsa_public_from_jwk_components(n_bytes, n_len,
					e_bytes, e_len, rebuilt_public, sizeof(rebuilt_public),
					&rebuilt_public_len) == 0);

			/* Sign with the rebuilt private, verify with the rebuilt
			 * public — fully decoupled from the original DER. */
			assert(jscrypto_rsa_pkcs1_v1_5_sign(rebuilt_private,
					rebuilt_private_len, JSCRYPTO_DIGEST_SHA256, message,
					sizeof(message), signature, sizeof(signature),
					&sig_len) == 0);
			assert(jscrypto_rsa_pkcs1_v1_5_verify(rebuilt_public,
					rebuilt_public_len, JSCRYPTO_DIGEST_SHA256, message,
					sizeof(message), signature, sig_len, &matches) == 0);
			assert(matches == 1);

			/* Public-only JWK extraction round-trips too. */
			assert(jscrypto_rsa_public_jwk_components(public_der,
					public_len, n_bytes, sizeof(n_bytes), &n_len,
					e_bytes, sizeof(e_bytes), &e_len) == 0);
			assert(e_len == 3);
		}

		/* 3072-bit spot check: generate → sign → verify with SHA-256.
		 * Slightly slower than 2048 but still well under a second. */
		{
			uint8_t priv3[4096];
			uint8_t pub3[1024];
			size_t priv3_len = 0;
			size_t pub3_len = 0;
			uint8_t sig3[512];
			size_t sig3_len = 0;

			assert(jscrypto_rsa_pkcs1_v1_5_generate(3072, priv3,
					sizeof(priv3), &priv3_len) == 0);
			assert(jscrypto_rsa_public_from_private(priv3, priv3_len, pub3,
					sizeof(pub3), &pub3_len) == 0);
			assert(jscrypto_rsa_pkcs1_v1_5_sign(priv3, priv3_len,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message), sig3,
					sizeof(sig3), &sig3_len) == 0);
			assert(sig3_len == 384);
			assert(jscrypto_rsa_pkcs1_v1_5_verify(pub3, pub3_len,
					JSCRYPTO_DIGEST_SHA256, message, sizeof(message), sig3,
					sig3_len, &matches) == 0);
			assert(matches == 1);
		}

		/* Reject unsupported modulus sizes. */
		{
			uint8_t tmp[16];
			size_t tmp_len = 0;

			assert(jscrypto_rsa_pkcs1_v1_5_generate(1024, tmp, sizeof(tmp),
					&tmp_len) == -1);
			assert(errno == ENOTSUP);
		}
	}

	/* RSA-PSS: shares key material with RSASSA-PKCS1-v1_5 but uses
	 * PSS padding + randomized salt. Round-trip sign/verify across
	 * all three hashes, confirm signatures randomize across calls,
	 * and cross-check that a PSS signature does NOT verify via the
	 * PKCS#1-v1.5 path. */
	{
		uint8_t private_der[4096];
		uint8_t public_der[1024];
		size_t private_len = 0;
		size_t public_len = 0;
		uint8_t message[32];
		uint8_t signature[512];
		uint8_t signature_b[512];
		size_t sig_len = 0;
		size_t sig_b_len = 0;
		size_t i;
		int matches = 0;

		for (i = 0; i < sizeof(message); i++) {
			message[i] = (uint8_t)(i * 7u);
		}
		assert(jscrypto_rsa_pkcs1_v1_5_generate(2048, private_der,
				sizeof(private_der), &private_len) == 0);
		assert(jscrypto_rsa_public_from_private(private_der, private_len,
				public_der, sizeof(public_der), &public_len) == 0);

		/* SHA-256 with default salt length (32 bytes). */
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(sig_len == 256);
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);

		/* Flip a signature byte -> verify resolves false. */
		signature[0] ^= 0x40u;
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 0);
		signature[0] ^= 0x40u;

		/* Randomization check: two signatures of the same payload
		 * must differ (PSS uses a random salt), but both must verify. */
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature_b, sizeof(signature_b), &sig_b_len) == 0);
		assert(sig_len == sig_b_len);
		assert(memcmp(signature, signature_b, sig_len) != 0);
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature_b, sig_b_len, &matches) == 0);
		assert(matches == 1);

		/* SHA-384 / SHA-512 round-trip. */
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA384, 48, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA384, 48, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA512, 64, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA512, 64, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);

		/* Cross-padding negative: sign with PSS, verify with PKCS#1-v1.5
		 * should reject (it won't be a valid PKCS#1 encoding). */
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA256, 32, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(jscrypto_rsa_pkcs1_v1_5_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 0);

		/* Salt length of 0 is spec-legal. */
		assert(jscrypto_rsa_pss_sign(private_der, private_len,
				JSCRYPTO_DIGEST_SHA256, 0, message, sizeof(message),
				signature, sizeof(signature), &sig_len) == 0);
		assert(jscrypto_rsa_pss_verify(public_der, public_len,
				JSCRYPTO_DIGEST_SHA256, 0, message, sizeof(message),
				signature, sig_len, &matches) == 0);
		assert(matches == 1);
	}

	/* SPKI / PKCS#8 format conversions for ECDSA P-256. Generate a
	 * keypair, export to SPKI / PKCS#8, reimport, and confirm the
	 * internal storage bytes round-trip. */
	{
		uint8_t private_key[32];
		uint8_t public_key[64];
		uint8_t spki[256];
		uint8_t pkcs8[256];
		uint8_t reimport_public[64];
		uint8_t reimport_private[32];
		size_t spki_len = 0;
		size_t pkcs8_len = 0;

		assert(jscrypto_ecdsa_p256_generate(private_key, public_key) == 0);

		assert(jscrypto_ecdsa_p256_public_to_spki(public_key, spki,
				sizeof(spki), &spki_len) == 0);
		/* SPKI for P-256 is ~91 bytes in practice. */
		assert(spki_len > 60 && spki_len < 120);
		assert(jscrypto_ecdsa_p256_spki_to_public(spki, spki_len,
				reimport_public) == 0);
		assert(memcmp(reimport_public, public_key, 64) == 0);

		assert(jscrypto_ecdsa_p256_private_to_pkcs8(private_key, pkcs8,
				sizeof(pkcs8), &pkcs8_len) == 0);
		assert(pkcs8_len > 100 && pkcs8_len < 200);
		assert(jscrypto_ecdsa_p256_pkcs8_to_private(pkcs8, pkcs8_len,
				reimport_private) == 0);
		assert(memcmp(reimport_private, private_key, 32) == 0);

		/* An RSA SPKI fed to the ECDSA parser must fail. We produce
		 * a short deliberately-malformed blob and check it errors. */
		{
			uint8_t bad[8] = { 0x00 };
			uint8_t scratch[64];

			assert(jscrypto_ecdsa_p256_spki_to_public(bad, sizeof(bad),
					scratch) == -1);
		}
	}

	/* SPKI / PKCS#8 for RSA (2048-bit). Generate a key, export both
	 * formats, reimport, confirm PKCS#1 DER byte-for-byte round-trip. */
	{
		uint8_t private_der[4096];
		uint8_t public_der[1024];
		uint8_t spki[1024];
		uint8_t pkcs8[4096];
		uint8_t reimport_public[1024];
		uint8_t reimport_private[4096];
		size_t private_len = 0;
		size_t public_len = 0;
		size_t spki_len = 0;
		size_t pkcs8_len = 0;
		size_t reimport_public_len = 0;
		size_t reimport_private_len = 0;

		assert(jscrypto_rsa_pkcs1_v1_5_generate(2048, private_der,
				sizeof(private_der), &private_len) == 0);
		assert(jscrypto_rsa_public_from_private(private_der, private_len,
				public_der, sizeof(public_der), &public_len) == 0);

		/* Public: PKCS#1 -> SPKI -> PKCS#1. SPKI adds a ~25-byte
		 * AlgorithmIdentifier wrapper plus a BIT STRING header. */
		assert(jscrypto_rsa_public_pkcs1_to_spki(public_der, public_len,
				spki, sizeof(spki), &spki_len) == 0);
		assert(spki_len > public_len && spki_len < public_len + 128);
		assert(jscrypto_rsa_spki_to_public_pkcs1(spki, spki_len,
				reimport_public, sizeof(reimport_public),
				&reimport_public_len) == 0);
		assert(reimport_public_len == public_len);
		assert(memcmp(reimport_public, public_der, public_len) == 0);

		/* Private: PKCS#1 -> PKCS#8 -> PKCS#1. */
		assert(jscrypto_rsa_private_pkcs1_to_pkcs8(private_der, private_len,
				pkcs8, sizeof(pkcs8), &pkcs8_len) == 0);
		assert(pkcs8_len > 0 && pkcs8_len <= sizeof(pkcs8));
		assert(jscrypto_rsa_pkcs8_to_private_pkcs1(pkcs8, pkcs8_len,
				reimport_private, sizeof(reimport_private),
				&reimport_private_len) == 0);
		assert(reimport_private_len == private_len);
		assert(memcmp(reimport_private, private_der, private_len) == 0);

		/* Sign/verify with the reimported key material works. */
		{
			uint8_t msg[16] = { 0 };
			uint8_t sig[256];
			size_t sig_len_b = 0;
			int matches = 0;

			assert(jscrypto_rsa_pkcs1_v1_5_sign(reimport_private,
					reimport_private_len, JSCRYPTO_DIGEST_SHA256, msg,
					sizeof(msg), sig, sizeof(sig), &sig_len_b) == 0);
			assert(jscrypto_rsa_pkcs1_v1_5_verify(reimport_public,
					reimport_public_len, JSCRYPTO_DIGEST_SHA256, msg,
					sizeof(msg), sig, sig_len_b, &matches) == 0);
			assert(matches == 1);
		}
	}

	/* Ed25519: generate, sign, verify round trip + RFC 8032 §7.1
	 * TEST 1 exact-bytes interop check. Ed25519 is deterministic
	 * so two signatures of the same message are byte-identical. */
	{
		uint8_t private_key[32];
		uint8_t public_key[32];
		uint8_t derived_public[32];
		uint8_t signature[64];
		uint8_t message[32];
		size_t i;
		int matches = 0;

		for (i = 0; i < sizeof(message); i++) {
			message[i] = (uint8_t)(i * 11u);
		}
		assert(jscrypto_ed25519_generate(private_key, public_key) == 0);
		assert(jscrypto_ed25519_public_from_private(private_key,
				derived_public) == 0);
		assert(memcmp(public_key, derived_public, 32) == 0);

		assert(jscrypto_ed25519_sign(private_key, message, sizeof(message),
				signature) == 0);
		assert(jscrypto_ed25519_verify(public_key, message, sizeof(message),
				signature, &matches) == 0);
		assert(matches == 1);

		/* Deterministic: sign twice, bytes match. */
		{
			uint8_t sig2[64];

			assert(jscrypto_ed25519_sign(private_key, message,
					sizeof(message), sig2) == 0);
			assert(memcmp(signature, sig2, 64) == 0);
		}

		/* Flip a signature byte -> verify resolves false. */
		signature[0] ^= 0x40u;
		assert(jscrypto_ed25519_verify(public_key, message, sizeof(message),
				signature, &matches) == 0);
		assert(matches == 0);
		signature[0] ^= 0x40u;

		/* Message tamper. */
		message[5] ^= 0x01u;
		assert(jscrypto_ed25519_verify(public_key, message, sizeof(message),
				signature, &matches) == 0);
		assert(matches == 0);
		message[5] ^= 0x01u;

		/* Different key -> verify resolves false. */
		{
			uint8_t other_private[32];
			uint8_t other_public[32];

			assert(jscrypto_ed25519_generate(other_private,
					other_public) == 0);
			assert(jscrypto_ed25519_verify(other_public, message,
					sizeof(message), signature, &matches) == 0);
			assert(matches == 0);
		}

		/* RFC 8032 §7.1 TEST 1: empty message, known seed, known
		 * public key, known signature. Proves byte-level interop
		 * with every other Ed25519 implementation on the planet. */
		{
			static const uint8_t rfc8032_seed[32] = {
				0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
				0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
				0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
				0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60
			};
			static const uint8_t rfc8032_public[32] = {
				0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
				0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
				0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
				0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a
			};
			static const uint8_t rfc8032_sig[64] = {
				0xe5, 0x56, 0x43, 0x00, 0xc3, 0x60, 0xac, 0x72,
				0x90, 0x86, 0xe2, 0xcc, 0x80, 0x6e, 0x82, 0x8a,
				0x84, 0x87, 0x7f, 0x1e, 0xb8, 0xe5, 0xd9, 0x74,
				0xd8, 0x73, 0xe0, 0x65, 0x22, 0x49, 0x01, 0x55,
				0x5f, 0xb8, 0x82, 0x15, 0x90, 0xa3, 0x3b, 0xac,
				0xc6, 0x1e, 0x39, 0x70, 0x1c, 0xf9, 0xb4, 0x6b,
				0xd2, 0x5b, 0xf5, 0xf0, 0x59, 0x5b, 0xbe, 0x24,
				0x65, 0x51, 0x41, 0x43, 0x8e, 0x7a, 0x10, 0x0b
			};
			uint8_t derived_public2[32];
			uint8_t rfc_sig_out[64];

			assert(jscrypto_ed25519_public_from_private(rfc8032_seed,
					derived_public2) == 0);
			assert(memcmp(derived_public2, rfc8032_public, 32) == 0);

			assert(jscrypto_ed25519_sign(rfc8032_seed, NULL, 0,
					rfc_sig_out) == 0);
			assert(memcmp(rfc_sig_out, rfc8032_sig, 64) == 0);

			assert(jscrypto_ed25519_verify(rfc8032_public, NULL, 0,
					rfc8032_sig, &matches) == 0);
			assert(matches == 1);
		}

		/* SPKI / PKCS#8 round trip. */
		{
			uint8_t spki[128];
			uint8_t pkcs8[128];
			uint8_t reimport_public[32];
			uint8_t reimport_private[32];
			size_t spki_len = 0;
			size_t pkcs8_len = 0;

			assert(jscrypto_ed25519_public_to_spki(public_key, spki,
					sizeof(spki), &spki_len) == 0);
			assert(spki_len > 30 && spki_len < 80);
			assert(jscrypto_ed25519_spki_to_public(spki, spki_len,
					reimport_public) == 0);
			assert(memcmp(reimport_public, public_key, 32) == 0);

			assert(jscrypto_ed25519_private_to_pkcs8(private_key, pkcs8,
					sizeof(pkcs8), &pkcs8_len) == 0);
			assert(pkcs8_len > 30 && pkcs8_len < 100);
			assert(jscrypto_ed25519_pkcs8_to_private(pkcs8, pkcs8_len,
					reimport_private) == 0);
			assert(memcmp(reimport_private, private_key, 32) == 0);
		}
	}
#else
	uint8_t byte = 0;
	int matches = 0;
	jscrypto_digest_algorithm_t algorithm;
	size_t len = 0;

	assert(jscrypto_random_bytes(&byte, 1) == -1);
	assert(errno == ENOTSUP);
	assert(jscrypto_random_uuid(&byte, 1, NULL) == -1);
	assert(errno == ENOTSUP || errno == ENOBUFS);
	assert(jscrypto_digest_algorithm_parse((const uint8_t *)"SHA-256", 7,
			&algorithm) == 0);
	assert(jscrypto_digest_length(algorithm, &len) == 0);
	assert(len == 32);
	assert(jscrypto_digest(algorithm, (const uint8_t *)"abc", 3, &byte, 1,
			NULL) == -1);
	assert(errno == ENOTSUP || errno == ENOBUFS);
	assert(jscrypto_hmac(algorithm, (const uint8_t *)"k", 1,
			(const uint8_t *)"abc", 3, &byte, 1, NULL) == -1);
	assert(errno == ENOTSUP || errno == ENOBUFS);
	assert(jscrypto_pbkdf2(algorithm, (const uint8_t *)"k", 1,
			(const uint8_t *)"s", 1, 1, &byte, 1) == -1);
	assert(errno == ENOTSUP);
	assert(jscrypto_hkdf(algorithm, (const uint8_t *)"k", 1,
			(const uint8_t *)"s", 1, (const uint8_t *)"i", 1, &byte, 1) == -1);
	assert(errno == ENOTSUP);
	assert(jscrypto_hmac_verify(algorithm, (const uint8_t *)"k", 1,
			(const uint8_t *)"abc", 3, &byte, 1, &matches) == -1);
	assert(errno == ENOTSUP || errno == ENOBUFS);
	assert(jscrypto_aes_gcm_encrypt((const uint8_t *)"0123456789abcdef", 16,
			(const uint8_t *)"abcdefghijkl", 12, NULL, 0, 128,
			(const uint8_t *)"abc", 3, &byte, 1, NULL) == -1);
		assert(errno == ENOTSUP || errno == ENOBUFS);
		assert(jscrypto_aes_gcm_decrypt((const uint8_t *)"0123456789abcdef", 16,
				(const uint8_t *)"abcdefghijkl", 12, NULL, 0, 128,
				aes_gcm_zero_ciphertext_16_tag,
				sizeof(aes_gcm_zero_ciphertext_16_tag), &byte, 1, NULL) == -1);
		assert(errno == ENOTSUP || errno == ENOBUFS);
	#endif

	return 0;
}

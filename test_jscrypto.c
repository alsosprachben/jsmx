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
	assert(jscrypto_hmac_verify(algorithm, (const uint8_t *)"k", 1,
			(const uint8_t *)"abc", 3, &byte, 1, &matches) == -1);
	assert(errno == ENOTSUP || errno == ENOBUFS);
#endif

	return 0;
}

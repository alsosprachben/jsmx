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
#else
	uint8_t byte = 0;
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
#endif

	return 0;
}

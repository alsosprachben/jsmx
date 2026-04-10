#include "jscrypto.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "utf8.h"

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

static uint8_t
jscrypto_hex_lower(uint8_t value)
{
	return (uint8_t)(value < 10 ? ('0' + value) : ('a' + (value - 10)));
}

static int
jscrypto_digest_algorithm_eq(const uint8_t *name, size_t len,
		const char *expected)
{
	size_t expected_len = strlen(expected);

	return len == expected_len && memcmp(name, expected, expected_len) == 0;
}

int
jscrypto_digest_algorithm_parse(const uint8_t *name, size_t len,
		jscrypto_digest_algorithm_t *algorithm_ptr)
{
	if (name == NULL || algorithm_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-1")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA1;
		return 0;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-256")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA256;
		return 0;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-384")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA384;
		return 0;
	}
	if (jscrypto_digest_algorithm_eq(name, len, "SHA-512")) {
		*algorithm_ptr = JSCRYPTO_DIGEST_SHA512;
		return 0;
	}
	errno = ENOTSUP;
	return -1;
}

int
jscrypto_digest_length(jscrypto_digest_algorithm_t algorithm, size_t *len_ptr)
{
	if (len_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	switch (algorithm) {
	case JSCRYPTO_DIGEST_SHA1:
		*len_ptr = 20;
		return 0;
	case JSCRYPTO_DIGEST_SHA256:
		*len_ptr = 32;
		return 0;
	case JSCRYPTO_DIGEST_SHA384:
		*len_ptr = 48;
		return 0;
	case JSCRYPTO_DIGEST_SHA512:
		*len_ptr = 64;
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
static const EVP_MD *
jscrypto_digest_evp(jscrypto_digest_algorithm_t algorithm)
{
	switch (algorithm) {
	case JSCRYPTO_DIGEST_SHA1:
		return EVP_sha1();
	case JSCRYPTO_DIGEST_SHA256:
		return EVP_sha256();
	case JSCRYPTO_DIGEST_SHA384:
		return EVP_sha384();
	case JSCRYPTO_DIGEST_SHA512:
		return EVP_sha512();
	default:
		return NULL;
	}
}
#endif

int
jscrypto_random_bytes(uint8_t *buf, size_t len)
{
	size_t offset = 0;

	if (buf == NULL) {
		errno = EINVAL;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)offset;
	(void)len;
	errno = ENOTSUP;
	return -1;
#else
	while (offset < len) {
		size_t chunk = len - offset;
		int rc;

		if (chunk > (size_t)INT_MAX) {
			chunk = (size_t)INT_MAX;
		}
		rc = RAND_bytes(buf + offset, (int)chunk);
		if (rc != 1) {
			errno = EIO;
			return -1;
		}
		offset += chunk;
	}
	return 0;
#endif
}

int
jscrypto_digest(jscrypto_digest_algorithm_t algorithm, const uint8_t *input,
		size_t input_len, uint8_t *output, size_t cap, size_t *len_ptr)
{
	size_t digest_len = 0;

	if ((input == NULL && input_len > 0) || (output == NULL && cap > 0)) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_digest_length(algorithm, &digest_len) < 0) {
		return -1;
	}
	if (len_ptr != NULL) {
		*len_ptr = digest_len;
	}
	if (output == NULL) {
		return 0;
	}
	if (cap < digest_len) {
		errno = ENOBUFS;
		return -1;
	}

#if !(JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL)
	(void)input;
	(void)input_len;
	errno = ENOTSUP;
	return -1;
#else
	{
		const EVP_MD *md = jscrypto_digest_evp(algorithm);
		unsigned int written = 0;

		if (md == NULL) {
			errno = EINVAL;
			return -1;
		}
		if (EVP_Digest(input, input_len, output, &written, md, NULL) != 1) {
			errno = EIO;
			return -1;
		}
		if ((size_t)written != digest_len) {
			errno = EIO;
			return -1;
		}
		return 0;
	}
#endif
}

int
jscrypto_random_uuid(uint8_t *buf, size_t cap, size_t *len_ptr)
{
	static const size_t uuid_len = 36;
	uint8_t bytes[16];
	size_t i;
	size_t out = 0;

	if (len_ptr != NULL) {
		*len_ptr = uuid_len;
	}
	if (buf == NULL && cap > 0) {
		errno = EINVAL;
		return -1;
	}
	if (jscrypto_random_bytes(bytes, sizeof(bytes)) < 0) {
		return -1;
	}
	bytes[6] = (uint8_t)((bytes[6] & 0x0f) | 0x40);
	bytes[8] = (uint8_t)((bytes[8] & 0x3f) | 0x80);
	if (buf == NULL) {
		return 0;
	}
	if (cap < uuid_len) {
		errno = ENOBUFS;
		return -1;
	}
	for (i = 0; i < sizeof(bytes); i++) {
		if (i == 4 || i == 6 || i == 8 || i == 10) {
			buf[out++] = '-';
		}
		buf[out++] = jscrypto_hex_lower((uint8_t)((bytes[i] >> 4) & 0x0f));
		buf[out++] = jscrypto_hex_lower((uint8_t)(bytes[i] & 0x0f));
	}
	return 0;
}

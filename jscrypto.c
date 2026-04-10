#include "jscrypto.h"

#include <errno.h>
#include <limits.h>
#include <string.h>

#include "utf8.h"

#if JSMX_WITH_CRYPTO && JSMX_CRYPTO_BACKEND_OPENSSL
#include <openssl/rand.h>
#endif

static uint8_t
jscrypto_hex_lower(uint8_t value)
{
	return (uint8_t)(value < 10 ? ('0' + value) : ('a' + (value - 10)));
}

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

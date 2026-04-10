#include <assert.h>
#include <errno.h>
#include <string.h>

#include "jscrypto.h"

static int
is_hex_lower(uint8_t ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

int
main(void)
{
#if JSMX_WITH_CRYPTO
	uint8_t bytes[32];
	uint8_t uuid[36];
	size_t len = 0;
	size_t i;
	int any_changed = 0;

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
#else
	uint8_t byte = 0;

	assert(jscrypto_random_bytes(&byte, 1) == -1);
	assert(errno == ENOTSUP);
	assert(jscrypto_random_uuid(&byte, 1, NULL) == -1);
	assert(errno == ENOTSUP || errno == ENOBUFS);
#endif

	return 0;
}

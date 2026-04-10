#include <errno.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-randomuuid-parity"

int
main(void)
{
	uint8_t storage[4096];
	uint8_t uuid[36];
	size_t len = 0;
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t value;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_crypto_random_uuid(&region, crypto_value, &value)
			== 0, SUITE, CASE_NAME, "failed to generate randomUUID");
	GENERATED_TEST_ASSERT(value.kind == JSVAL_KIND_STRING, SUITE, CASE_NAME,
			"expected UUID string result");
	GENERATED_TEST_ASSERT(jsval_string_copy_utf8(&region, value, uuid,
			sizeof(uuid), &len) == 0, SUITE, CASE_NAME,
			"failed to copy UUID string");
	GENERATED_TEST_ASSERT(len == 36, SUITE, CASE_NAME,
			"expected 36-byte UUID result");
	GENERATED_TEST_ASSERT(uuid[8] == '-' && uuid[13] == '-' && uuid[18] == '-'
			&& uuid[23] == '-', SUITE, CASE_NAME,
			"unexpected UUID separator layout");
	GENERATED_TEST_ASSERT(uuid[14] == '4', SUITE, CASE_NAME,
			"expected RFC 4122 version 4 UUID");
	GENERATED_TEST_ASSERT(uuid[19] == '8' || uuid[19] == '9'
			|| uuid[19] == 'a' || uuid[19] == 'b',
			SUITE, CASE_NAME, "unexpected UUID variant nibble");
	return generated_test_pass(SUITE, CASE_NAME);
}

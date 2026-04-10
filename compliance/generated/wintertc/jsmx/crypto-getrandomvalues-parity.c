#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-getrandomvalues-parity"

int
main(void)
{
	uint8_t storage[8192];
	uint8_t before[16];
	uint8_t after[16];
	size_t len = 0;
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t typed_array;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			16, &typed_array) == 0, SUITE, CASE_NAME,
			"failed to allocate Uint8Array");
	GENERATED_TEST_ASSERT(jsval_typed_array_copy_bytes(&region, typed_array,
			before, sizeof(before), &len) == 0 && len == 16,
			SUITE, CASE_NAME, "failed to copy pre-random bytes");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_crypto_get_random_values(&region, crypto_value,
			typed_array, &result, &error) == 0, SUITE, CASE_NAME,
			"getRandomValues failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, typed_array) == 1,
			SUITE, CASE_NAME,
			"expected getRandomValues to return the same typed array");
	GENERATED_TEST_ASSERT(jsval_typed_array_copy_bytes(&region, typed_array,
			after, sizeof(after), &len) == 0 && len == 16,
			SUITE, CASE_NAME, "failed to copy randomized bytes");
	GENERATED_TEST_ASSERT(memcmp(before, after, sizeof(after)) != 0,
			SUITE, CASE_NAME,
			"expected randomized bytes to differ from the zero-filled buffer");
	return generated_test_pass(SUITE, CASE_NAME);
}

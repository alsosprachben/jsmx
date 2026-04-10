#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/crypto-getrandomvalues-errors"

int
main(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t float_array;
	jsval_t large_array;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_crypto_new(&region, &crypto_value) == 0,
			SUITE, CASE_NAME, "failed to allocate crypto object");
	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_FLOAT32,
			4, &float_array) == 0, SUITE, CASE_NAME,
			"failed to allocate Float32Array");
	memset(&error, 0, sizeof(error));
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_crypto_get_random_values(&region, crypto_value,
			float_array, &result, &error) < 0, SUITE, CASE_NAME,
			"expected Float32Array rejection");
	GENERATED_TEST_ASSERT(errno == EINVAL && error.kind == JSMETHOD_ERROR_TYPE
			&& strcmp(error.message, "TypeMismatchError") == 0,
			SUITE, CASE_NAME,
			"expected TypeMismatchError for Float32Array");

	GENERATED_TEST_ASSERT(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
			65537, &large_array) == 0, SUITE, CASE_NAME,
			"failed to allocate oversized Uint8Array");
	memset(&error, 0, sizeof(error));
	errno = 0;
	GENERATED_TEST_ASSERT(jsval_crypto_get_random_values(&region, crypto_value,
			large_array, &result, &error) < 0, SUITE, CASE_NAME,
			"expected quota rejection");
	GENERATED_TEST_ASSERT(errno == EOVERFLOW
			&& error.kind == JSMETHOD_ERROR_RANGE
			&& strcmp(error.message, "QuotaExceededError") == 0,
			SUITE, CASE_NAME,
			"expected QuotaExceededError for oversized typed array");
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.8_A3_T4"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/bitwise-not/S11.4.8_A3_T4.js
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_undefined(), &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~void 0");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~void 0") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~void 0");
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_null(), &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~null");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~null") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~null");

	return generated_test_pass(SUITE, CASE_NAME);
}

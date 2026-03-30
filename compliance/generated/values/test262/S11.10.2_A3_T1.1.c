#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.10.2_A3_T1.1"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/bitwise-xor/S11.10.2_A3_T1.1.js
	 *
	 * This idiomatic flattened translation preserves the primitive boolean
	 * check. The Boolean-object checks in CHECK#2-CHECK#4 are intentionally
	 * omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bitwise_xor(&region, jsval_bool(1),
			jsval_bool(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower true ^ true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "true ^ true") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for true ^ true");

	return generated_test_pass(SUITE, CASE_NAME);
}

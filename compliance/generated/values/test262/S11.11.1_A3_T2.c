#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.11.1_A3_T2"

int
main(void)
{
	uint8_t storage[128];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-and/S11.11.1_A3_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive numeric
	 * cases for signed zero and NaN. The Number-object cases are intentionally
	 * omitted above the current flattened `jsval` layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	result = generated_logical_and(&region, jsval_number(-0.0),
			jsval_number(-1.0));
	GENERATED_TEST_ASSERT(generated_expect_value_negative_zero(result, SUITE,
			CASE_NAME, "-0 && -1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -0 && -1");

	result = generated_logical_and(&region, jsval_number(0.0),
			jsval_number(-1.0));
	GENERATED_TEST_ASSERT(generated_expect_value_positive_zero(result, SUITE,
			CASE_NAME, "0 && -1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 0 && -1");

	result = generated_logical_and(&region, jsval_number(NAN),
			jsval_number(1.0));
	GENERATED_TEST_ASSERT(generated_expect_value_nan(result, SUITE, CASE_NAME,
			"NaN && 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN && 1");

	return generated_test_pass(SUITE, CASE_NAME);
}

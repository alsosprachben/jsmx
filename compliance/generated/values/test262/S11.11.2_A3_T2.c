#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.11.2_A3_T2"

int
main(void)
{
	uint8_t storage[128];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-or/S11.11.2_A3_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive numeric
	 * signed-zero cases. The Number-object cases are intentionally omitted
	 * above the current flattened `jsval` layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	result = generated_logical_or(&region, jsval_number(0.0),
			jsval_number(-0.0));
	GENERATED_TEST_ASSERT(generated_expect_value_negative_zero(result, SUITE,
			CASE_NAME, "0 || -0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 0 || -0");

	result = generated_logical_or(&region, jsval_number(-0.0),
			jsval_number(0.0));
	GENERATED_TEST_ASSERT(generated_expect_value_positive_zero(result, SUITE,
			CASE_NAME, "-0 || 0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -0 || 0");

	return generated_test_pass(SUITE, CASE_NAME);
}

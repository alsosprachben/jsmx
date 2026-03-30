#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.7_A4.2"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/unary-minus/S11.4.7_A4.2.js
	 *
	 * This is a direct flattened translation of signed-zero negation.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_unary_minus(&region, jsval_number(0.0), &result) == 0,
			SUITE, CASE_NAME, "failed to lower -(+0)");
	GENERATED_TEST_ASSERT(generated_expect_value_negative_zero(result, SUITE,
			CASE_NAME, "-(+0)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -(+0)");

	GENERATED_TEST_ASSERT(jsval_unary_minus(&region, jsval_number(-0.0), &result) == 0,
			SUITE, CASE_NAME, "failed to lower -(-0)");
	GENERATED_TEST_ASSERT(generated_expect_value_positive_zero(result, SUITE,
			CASE_NAME, "-(-0)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -(-0)");

	return generated_test_pass(SUITE, CASE_NAME);
}

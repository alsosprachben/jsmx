#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.3_A3_T2"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/unary-plus/S9.3_A3_T2.js
	 *
	 * This is a direct flattened translation of unary plus over primitive
	 * booleans.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_unary_plus(&region, jsval_bool(0), &result) == 0,
			SUITE, CASE_NAME, "failed to lower +(false)");
	GENERATED_TEST_ASSERT(generated_expect_value_positive_zero(result, SUITE,
			CASE_NAME, "+false") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for +(false)");

	GENERATED_TEST_ASSERT(jsval_unary_plus(&region, jsval_bool(1), &result) == 0,
			SUITE, CASE_NAME, "failed to lower +(true)");
	GENERATED_TEST_ASSERT(generated_expect_value_strict_eq(&region, result,
			jsval_number(1.0), SUITE, CASE_NAME,
			"+true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for +(true)");

	return generated_test_pass(SUITE, CASE_NAME);
}

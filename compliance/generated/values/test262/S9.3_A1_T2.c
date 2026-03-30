#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.3_A1_T2"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	jsval_t result;
	jsval_t x;

	/*
	 * Generated from test262:
	 * test/language/expressions/unary-plus/S9.3_A1_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive undefined
	 * cases for unary plus, including a local slot standing in for the
	 * upstream eval-produced undefined value.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	x = jsval_undefined();

	GENERATED_TEST_ASSERT(jsval_unary_plus(&region, jsval_undefined(), &result) == 0,
			SUITE, CASE_NAME, "failed to lower +(undefined)");
	GENERATED_TEST_ASSERT(generated_expect_value_nan(result, SUITE, CASE_NAME,
			"+undefined") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for +(undefined)");

	GENERATED_TEST_ASSERT(jsval_unary_plus(&region, jsval_undefined(), &result) == 0,
			SUITE, CASE_NAME, "failed to lower +(void 0)");
	GENERATED_TEST_ASSERT(generated_expect_value_nan(result, SUITE, CASE_NAME,
			"+void 0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for +(void 0)");

	GENERATED_TEST_ASSERT(jsval_unary_plus(&region, x, &result) == 0,
			SUITE, CASE_NAME, "failed to lower +(local undefined)");
	GENERATED_TEST_ASSERT(generated_expect_value_nan(result, SUITE, CASE_NAME,
			"+local undefined") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for +(local undefined)");

	return generated_test_pass(SUITE, CASE_NAME);
}

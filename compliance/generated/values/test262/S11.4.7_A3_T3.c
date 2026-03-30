#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.7_A3_T3"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t one;
	jsval_t bad;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/unary-minus/S11.4.7_A3_T3.js
	 *
	 * This idiomatic flattened translation preserves the primitive string
	 * cases. The wrapper-object case is intentionally omitted at the current
	 * flattened layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one) == 0, SUITE, CASE_NAME, "failed to construct \"1\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&bad) == 0, SUITE, CASE_NAME, "failed to construct \"x\"");

	GENERATED_TEST_ASSERT(jsval_unary_minus(&region, one, &result) == 0,
			SUITE, CASE_NAME, "failed to lower -\"1\"");
	GENERATED_TEST_ASSERT(generated_expect_value_strict_eq(&region, result,
			jsval_number(-1.0), SUITE, CASE_NAME,
			"-\"1\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -\"1\"");

	GENERATED_TEST_ASSERT(jsval_unary_minus(&region, bad, &result) == 0,
			SUITE, CASE_NAME, "failed to lower -\"x\"");
	GENERATED_TEST_ASSERT(generated_expect_value_nan(result, SUITE, CASE_NAME,
			"-\"x\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -\"x\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

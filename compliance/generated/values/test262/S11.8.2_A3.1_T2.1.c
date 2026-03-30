#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.8.2_A3.1_T2.1"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	int result;

	/*
	 * Generated from test262:
	 * test/language/expressions/greater-than/S11.8.2_A3.1_T2.1.js
	 *
	 * This idiomatic flattened translation preserves the primitive
	 * boolean/number relational checks. The wrapper-object checks are
	 * intentionally omitted at the current flattened layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_greater_than(&region, jsval_bool(1),
			jsval_number(1.0), &result) == 0, SUITE, CASE_NAME,
			"failed to lower true > 1");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "true > 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for true > 1");
	GENERATED_TEST_ASSERT(jsval_greater_than(&region, jsval_number(1.0),
			jsval_bool(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower 1 > true");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "1 > true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for 1 > true");

	return generated_test_pass(SUITE, CASE_NAME);
}

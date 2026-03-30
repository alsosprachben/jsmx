#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.8.1_A3.1_T2.9"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	int result;

	/*
	 * Generated from test262:
	 * test/language/expressions/less-than/S11.8.1_A3.1_T2.9.js
	 *
	 * This idiomatic flattened translation preserves the primitive
	 * boolean/null relational checks. The wrapper-object checks are
	 * intentionally omitted at the current flattened layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_bool(1),
			jsval_null(), &result) == 0, SUITE, CASE_NAME,
			"failed to lower true < null");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "true < null") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for true < null");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_null(),
			jsval_bool(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower null < true");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 1, SUITE,
			CASE_NAME, "null < true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for null < true");

	return generated_test_pass(SUITE, CASE_NAME);
}

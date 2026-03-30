#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.8.1_A3.2_T1.1"

int
main(void)
{
	uint8_t storage[128];
	jsval_region_t region;
	jsval_t one_string;
	jsval_t x_string;
	int result;

	/*
	 * Generated from test262:
	 * test/language/expressions/less-than/S11.8.1_A3.2_T1.1.js
	 *
	 * This idiomatic flattened translation preserves the primitive
	 * string/string relational checks. The String-object checks are
	 * intentionally omitted at the current flattened layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1", 1, &one_string) == 0,
			SUITE, CASE_NAME, "failed to construct \"1\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"x", 1, &x_string) == 0,
			SUITE, CASE_NAME, "failed to construct \"x\"");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, one_string, one_string,
			&result) == 0, SUITE, CASE_NAME,
			"failed to lower \"1\" < \"1\"");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "\"1\" < \"1\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"1\" < \"1\"");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, x_string, one_string,
			&result) == 0, SUITE, CASE_NAME,
			"failed to lower \"x\" < \"1\"");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "\"x\" < \"1\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"x\" < \"1\"");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, one_string, x_string,
			&result) == 0, SUITE, CASE_NAME,
			"failed to lower \"1\" < \"x\"");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 1, SUITE,
			CASE_NAME, "\"1\" < \"x\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"1\" < \"x\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.8_A3_T3"

int
main(void)
{
	uint8_t storage[512];
	jsval_region_t region;
	jsval_t one_string;
	jsval_t bad_string;
	jsval_t empty_string;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/bitwise-not/S11.4.8_A3_T3.js
	 *
	 * This idiomatic flattened translation preserves the primitive string
	 * checks. The String-object checks in CHECK#2 and CHECK#5 are intentionally
	 * omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"1\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&bad_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"x\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty_string) == 0, SUITE, CASE_NAME,
			"failed to construct empty string operand");

	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, one_string, &result) == 0,
			SUITE, CASE_NAME, "failed to lower ~\"1\"");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -2.0, SUITE,
			CASE_NAME, "~\"1\"") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~\"1\"");
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, bad_string, &result) == 0,
			SUITE, CASE_NAME, "failed to lower ~\"x\"");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~\"x\"") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~\"x\"");
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, empty_string, &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~\"\"");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~\"\"") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~\"\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

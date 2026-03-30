#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/endsWith/coerced-values-of-position"

int
main(void)
{
	jsmethod_error_t error;
	int result;
	static const uint8_t str[] = "The future is cool!";

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/endsWith/coerced-values-of-position.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			1, jsmethod_value_number(NAN), &error) == 0,
			SUITE, CASE_NAME, "failed to lower endsWith('', NaN)");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 1, SUITE,
			CASE_NAME, "str.endsWith('', NaN)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN");
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"The future", 10),
			1, jsmethod_value_number(10.4), &error) == 0,
			SUITE, CASE_NAME, "failed to lower endsWith at 10.4");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 1, SUITE,
			CASE_NAME, "str.endsWith('The future', 10.4)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for 10.4");
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"T", 1),
			1, jsmethod_value_bool(1), &error) == 0,
			SUITE, CASE_NAME, "failed to lower endsWith at true");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 1, SUITE,
			CASE_NAME, "str.endsWith('T', true)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for true");
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"The future", 10),
			1, jsmethod_value_string_utf8((const uint8_t *)"10", 2), &error) == 0,
			SUITE, CASE_NAME, "failed to lower endsWith at \"10\"");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 1, SUITE,
			CASE_NAME, "str.endsWith('The future', \"10\")")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for \"10\"");
	return generated_test_pass(SUITE, CASE_NAME);
}

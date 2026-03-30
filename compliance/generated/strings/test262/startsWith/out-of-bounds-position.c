#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/startsWith/out-of-bounds-position"

int
main(void)
{
	jsmethod_error_t error;
	int result;
	static const uint8_t str[] = "The future is cool!";

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/startsWith/out-of-bounds-position.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_starts_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"!", 1),
			1, jsmethod_value_number((double)(sizeof(str) - 1)), &error) == 0,
			SUITE, CASE_NAME, "failed to lower startsWith at str.length");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 0, SUITE,
			CASE_NAME, "str.startsWith(\"!\", str.length)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result at str.length");
	GENERATED_TEST_ASSERT(jsmethod_string_starts_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"!", 1),
			1, jsmethod_value_number(100.0), &error) == 0,
			SUITE, CASE_NAME, "failed to lower startsWith at 100");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 0, SUITE,
			CASE_NAME, "str.startsWith(\"!\", 100)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result at 100");
	GENERATED_TEST_ASSERT(jsmethod_string_starts_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"!", 1),
			1, jsmethod_value_number(INFINITY), &error) == 0,
			SUITE, CASE_NAME, "failed to lower startsWith at Infinity");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 0, SUITE,
			CASE_NAME, "str.startsWith(\"!\", Infinity)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result at Infinity");
	GENERATED_TEST_ASSERT(jsmethod_string_starts_with(&result,
			jsmethod_value_string_utf8(str, sizeof(str) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"The future", 10),
			1, jsmethod_value_number(-1.0), &error) == 0,
			SUITE, CASE_NAME, "failed to lower startsWith at -1");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 1, SUITE,
			CASE_NAME, "str.startsWith(\"The future\", -1)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result at -1");
	return generated_test_pass(SUITE, CASE_NAME);
}

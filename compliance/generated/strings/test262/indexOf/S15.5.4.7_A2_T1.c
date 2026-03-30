#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/indexOf/S15.5.4.7_A2_T1"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/indexOf/S15.5.4.7_A2_T1.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_index_of(&index,
			jsmethod_value_string_utf8((const uint8_t *)"abcd", 4),
			jsmethod_value_string_utf8((const uint8_t *)"abcdab", 6),
			0, jsmethod_value_undefined(), &error) == 0,
			SUITE, CASE_NAME, "failed to lower \"abcd\".indexOf(\"abcdab\")");
	GENERATED_TEST_ASSERT(generated_expect_search_index(index, -1, SUITE,
			CASE_NAME, "\"abcd\".indexOf(\"abcdab\")")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for longer searchString");
	return generated_test_pass(SUITE, CASE_NAME);
}

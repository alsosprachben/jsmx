#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/lastIndexOf/not-a-substring"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/lastIndexOf/not-a-substring.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_last_index_of(&index,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3),
			jsmethod_value_string_utf8((const uint8_t *)"d", 1),
			0, jsmethod_value_undefined(), &error) == 0, SUITE, CASE_NAME,
			"failed to lower \"abc\".lastIndexOf(\"d\")");
	GENERATED_TEST_ASSERT(generated_expect_search_index(index, -1, SUITE,
			CASE_NAME, "\"abc\".lastIndexOf(\"d\")") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for non-substring");
	return generated_test_pass(SUITE, CASE_NAME);
}

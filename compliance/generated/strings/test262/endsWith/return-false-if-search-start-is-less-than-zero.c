#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/endsWith/return-false-if-search-start-is-less-than-zero"

int
main(void)
{
	jsmethod_error_t error;
	int result;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/endsWith/return-false-if-search-start-is-less-than-zero.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8((const uint8_t *)"web", 3),
			jsmethod_value_string_utf8((const uint8_t *)"w", 1),
			1, jsmethod_value_number(0.0), &error) == 0,
			SUITE, CASE_NAME, "failed to lower \"web\".endsWith(\"w\", 0)");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 0, SUITE,
			CASE_NAME, "\"web\".endsWith(\"w\", 0)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for \"web\".endsWith(\"w\", 0)");
	GENERATED_TEST_ASSERT(jsmethod_string_ends_with(&result,
			jsmethod_value_string_utf8((const uint8_t *)"Bob", 3),
			jsmethod_value_string_utf8((const uint8_t *)"  Bob", 5),
			0, jsmethod_value_undefined(), &error) == 0,
			SUITE, CASE_NAME, "failed to lower \"Bob\".endsWith(\"  Bob\")");
	GENERATED_TEST_ASSERT(generated_expect_search_bool(result, 0, SUITE,
			CASE_NAME, "\"Bob\".endsWith(\"  Bob\")")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for longer suffix");
	return generated_test_pass(SUITE, CASE_NAME);
}

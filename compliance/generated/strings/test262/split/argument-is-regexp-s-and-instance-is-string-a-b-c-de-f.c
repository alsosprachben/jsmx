#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/argument-is-regexp-s-and-instance-is-string-a-b-c-de-f"

int
main(void)
{
	static const char *expected[] = {"a", "b", "c", "de", "f"};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t pattern;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/split/argument-is-regexp-s-and-instance-is-string-a-b-c-de-f.js
	 *
	 * The upstream boxed String receiver is intentionally collapsed to its
	 * flattenable primitive string value while preserving the regex split result.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a b c de f", 10, &text) == 0,
			SUITE, CASE_NAME, "failed to create input string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"\\s", 2, &pattern) == 0,
			SUITE, CASE_NAME, "failed to create regex pattern");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &regex, &error) == 0,
			SUITE, CASE_NAME, "failed to create regex");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1, regex, 0,
			jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "regex split failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			expected, GENERATED_LEN(expected), SUITE, CASE_NAME, "#1") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex split mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

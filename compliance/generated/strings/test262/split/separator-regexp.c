#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/separator-regexp"

static int
check_case(jsval_region_t *region, const char *input, const char *pattern,
		const char *const *expected, size_t expected_len, const char *label)
{
	jsval_t text;
	jsval_t pattern_value;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	if (jsval_string_new_utf8(region, (const uint8_t *)input, strlen(input),
			&text) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to create input string: %s", label,
				strerror(errno));
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)pattern,
			strlen(pattern), &pattern_value) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to create regex pattern: %s", label,
				strerror(errno));
	}
	if (jsval_regexp_new(region, pattern_value, 0, jsval_undefined(),
			&regex, &error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to create regex: errno=%d kind=%d", label,
				errno, (int)error.kind);
	}
	if (jsval_method_string_split(region, text, 1, regex, 0,
			jsval_undefined(), &result, &error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: regex split failed: errno=%d kind=%d", label,
				errno, (int)error.kind);
	}
	return generated_expect_split_array_strings(region, result, expected,
			expected_len, SUITE, CASE_NAME, label);
}

int
main(void)
{
	static const char *expected0[] = {"x"};
	static const char *expected1[] = {"", ""};

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/split/separator-regexp.js
	 *
	 * This is an idiomatic flattened translation of a representative subset of
	 * the upstream file. It preserves the split edge cases that matter to the
	 * current runtime boundary: zero-width begin/end, consuming matches, and
	 * zero-width progress through /(?:)/.
	 */

	uint8_t storage[32768];
	jsval_region_t region;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(check_case(&region, "x", "^", expected0,
			GENERATED_LEN(expected0), "/^/") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex /^/ mismatch");
	GENERATED_TEST_ASSERT(check_case(&region, "x", "$", expected0,
			GENERATED_LEN(expected0), "/$/") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex /$/ mismatch");
	GENERATED_TEST_ASSERT(check_case(&region, "x", ".?", expected1,
			GENERATED_LEN(expected1), "/.?/") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex /.?/ mismatch");
	GENERATED_TEST_ASSERT(check_case(&region, "x", ".*?", expected0,
			GENERATED_LEN(expected0), "/.*?/") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex /.*?/ mismatch");
	GENERATED_TEST_ASSERT(check_case(&region, "x", "(?:)", expected0,
			GENERATED_LEN(expected0), "/(?:)/") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex /(?:)/ mismatch");
	GENERATED_TEST_ASSERT(check_case(&region, "x", ".", expected1,
			GENERATED_LEN(expected1), "/./") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "regex /./ mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

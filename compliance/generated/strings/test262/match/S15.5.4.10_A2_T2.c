#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/match/S15.5.4.10_A2_T2"

int
main(void)
{
	static const char *expected[] = {"34", "34", "34"};
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t text;
	jsval_t pattern;
	jsval_t flags;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"343443444", 9, &text) == 0,
			SUITE, CASE_NAME, "string build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"34", 2, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"g", 1, &flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, flags, &regex,
			&error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, text, 1, regex,
			&result, &error) == 0, SUITE, CASE_NAME, "match failed");
	GENERATED_TEST_ASSERT(generated_expect_match_array_strings(&region, result,
			expected, 3, SUITE, CASE_NAME, "global match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "global match mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

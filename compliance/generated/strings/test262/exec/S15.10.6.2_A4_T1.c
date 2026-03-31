#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/S15.10.6.2_A4_T1"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t flags;
	jsval_t regex;
	jsval_t input;
	jsval_t result;
	size_t last_index;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"(?:ab|cd)\\d?", 11, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"g", 1, &flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1, flags, &regex,
			&error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"aacd2233ab12nm444ab42", 21, &input) == 0,
			SUITE, CASE_NAME, "input build failed");

	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, input, &result,
			&error) == 0, SUITE, CASE_NAME, "first exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"cd2", SUITE, CASE_NAME, "first match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 2.0, SUITE, CASE_NAME, "first index") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first index mismatch");
	GENERATED_TEST_ASSERT(jsval_regexp_get_last_index(&region, regex,
			&last_index) == 0 && last_index == 5,
			SUITE, CASE_NAME, "expected lastIndex=5 after first exec");

	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, regex, 12) == 0,
			SUITE, CASE_NAME, "lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, input, &result,
			&error) == 0, SUITE, CASE_NAME, "second exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"ab4", SUITE, CASE_NAME, "second match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 17.0, SUITE, CASE_NAME, "second index") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second index mismatch");
	GENERATED_TEST_ASSERT(jsval_regexp_get_last_index(&region, regex,
			&last_index) == 0 && last_index == 20,
			SUITE, CASE_NAME, "expected lastIndex=20 after second exec");
	return generated_test_pass(SUITE, CASE_NAME);
}

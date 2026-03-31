#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/match/S15.5.4.10_A2_T10"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t pattern;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"Boston, MA 02134", 16, &text) == 0,
			SUITE, CASE_NAME, "text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"([\\d]{5})([-\\ ]?[\\d]{4})?$", 26, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, regex, 11) == 0,
			SUITE, CASE_NAME, "lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, text, 1, regex,
			&result, &error) == 0, SUITE, CASE_NAME, "match failed");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"length", 3.0, SUITE, CASE_NAME, "length") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "length mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 11.0, SUITE, CASE_NAME, "index") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"02134", SUITE, CASE_NAME, "match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "1",
			"02134", SUITE, CASE_NAME, "match[1]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[1] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_undefined_prop(&region, result,
			"2", SUITE, CASE_NAME, "match[2]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[2] mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

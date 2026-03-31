#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/match/S15.5.4.10_A1_T14"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t text;
	jsval_t pattern;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"77", 2, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"ABBABABAB77BBAA", 14, &text) == 0,
			SUITE, CASE_NAME, "text build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, text, 1, regex,
			&result, &error) == 0, SUITE, CASE_NAME, "match failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"77", SUITE, CASE_NAME, "match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[0] mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

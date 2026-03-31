#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/S15.10.6.2_A1_T10"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1|12", 4, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, jsval_number(1.01),
			&result, &error) == 0, SUITE, CASE_NAME, "exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"1", SUITE, CASE_NAME, "match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 0.0, SUITE, CASE_NAME, "index") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"input", "1.01", SUITE, CASE_NAME, "input") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "input mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

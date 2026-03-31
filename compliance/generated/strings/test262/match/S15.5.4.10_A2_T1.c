#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/match/S15.5.4.10_A2_T1"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t text;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1234567890", 10, &text) == 0,
			SUITE, CASE_NAME, "string build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, text, 1,
			jsval_number(3.0), &result, &error) == 0,
			SUITE, CASE_NAME, "match failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result, "0",
			"3", SUITE, CASE_NAME, "match[0]") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"length", 1.0, SUITE, CASE_NAME, "length") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "length mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 2.0, SUITE, CASE_NAME, "index") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"input", "1234567890", SUITE, CASE_NAME, "input")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "input mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

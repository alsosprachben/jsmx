#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/test/y-set-lastindex"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t regex;
	jsval_t input;
	jsmethod_error_t error;
	int test_result = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "y",
			&regex, &error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"abc", 3, &input) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_test(&region, regex, input,
			&test_result, &error) == 0, SUITE, CASE_NAME, "test failed");
	GENERATED_TEST_ASSERT(test_result == 1,
			SUITE, CASE_NAME, "expected sticky hit");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region, regex,
			JSREGEX_FLAG_STICKY, 0, 3, SUITE, CASE_NAME, "regex state")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "state mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

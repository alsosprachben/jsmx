#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/constructor/valid-flags-y"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t regex;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "y",
			&regex, &error) == 0, SUITE, CASE_NAME, "y should pass");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "y",
			SUITE, CASE_NAME, "flags y") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags y mismatch");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "gy",
			&regex, &error) == 0, SUITE, CASE_NAME, "gy should pass");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "iy",
			&regex, &error) == 0, SUITE, CASE_NAME, "iy should pass");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "my",
			&regex, &error) == 0, SUITE, CASE_NAME, "my should pass");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "uy",
			&regex, &error) == 0, SUITE, CASE_NAME, "uy should pass");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "gimuy",
			&regex, &error) == 0, SUITE, CASE_NAME, "gimuy should pass");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, regex, "gimuy",
			SUITE, CASE_NAME, "flags gimuy") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "flags gimuy mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

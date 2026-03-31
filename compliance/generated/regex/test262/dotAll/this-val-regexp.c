#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/dotAll/this-val-regexp"

#define CHECK_DOTALL(flag_text, expected_value, label) \
	do { \
		GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", flag_text, \
				&regex, &error) == 0, SUITE, CASE_NAME, label " build failed"); \
		GENERATED_TEST_ASSERT(generated_expect_regexp_has_flag(&region, regex, \
				JSREGEX_FLAG_DOT_ALL, expected_value, SUITE, CASE_NAME, label) \
				== GENERATED_TEST_PASS, SUITE, CASE_NAME, label " mismatch"); \
	} while (0)

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t regex;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	CHECK_DOTALL(NULL, 0, "dotAll none");
	CHECK_DOTALL("i", 0, "dotAll i");
	CHECK_DOTALL("g", 0, "dotAll g");
	CHECK_DOTALL("y", 0, "dotAll y");
	CHECK_DOTALL("m", 0, "dotAll m");
	CHECK_DOTALL("s", 1, "dotAll s");
	CHECK_DOTALL("is", 1, "dotAll is");
	CHECK_DOTALL("sg", 1, "dotAll sg");
	CHECK_DOTALL("sy", 1, "dotAll sy");
	CHECK_DOTALL("ms", 1, "dotAll ms");
	return generated_test_pass(SUITE, CASE_NAME);
}

#undef CHECK_DOTALL

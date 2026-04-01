#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/separator-undef"

int
main(void)
{
	static const char *expected[] = {"undefined is not a function"};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)expected[0], strlen(expected[0]), &text) == 0,
			SUITE, CASE_NAME, "failed to create input string");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 0,
			jsval_undefined(), 0, jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "implicit undefined separator split failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			expected, 1, SUITE, CASE_NAME, "implicit") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "implicit undefined separator mismatch");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, text, 1,
			jsval_undefined(), 0, jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "explicit undefined separator split failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region, result,
			expected, 1, SUITE, CASE_NAME, "explicit") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "explicit undefined separator mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

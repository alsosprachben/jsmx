#include "compliance/generated/regex_core_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/searchValue-flags-no-g-throws"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t object_value;
	jsval_t pattern;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/searchValue-flags-no-g-throws.js
	 *
	 * This translation preserves the core non-global RegExp rejection. The
	 * upstream reflective flags-property override samples are intentionally
	 * omitted because the current regex value model is semantic rather than
	 * reflective.
	 */

	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &object_value) == 0,
			SUITE, CASE_NAME, "object build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)".", 1, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all(&region,
			object_value, regex, object_value, &result, &error) == -1,
			SUITE, CASE_NAME, "expected non-global regex to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for non-global regex, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

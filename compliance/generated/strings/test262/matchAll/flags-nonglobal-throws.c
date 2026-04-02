#include "compliance/generated/regex_core_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/matchAll/flags-nonglobal-throws"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t regex;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/matchAll/flags-nonglobal-throws.js
	 *
	 * This translation preserves the non-global RegExp rejection through the
	 * semantic regex value model. The upstream reflective flags-property
	 * override sample is intentionally omitted because the current runtime
	 * exposes semantic flag state rather than reflective property overrides.
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"", 0, &text) == 0,
			SUITE, CASE_NAME, "text build failed");

	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "a", NULL,
			&regex, &error) == 0,
			SUITE, CASE_NAME, "plain regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, text, 1,
			regex, &result, &error) == -1,
			SUITE, CASE_NAME, "expected plain non-global regex rejection");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for plain regex, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "a", "i",
			&regex, &error) == 0,
			SUITE, CASE_NAME, "ignoreCase regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, text, 1,
			regex, &result, &error) == -1,
			SUITE, CASE_NAME, "expected ignoreCase non-global rejection");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for ignoreCase regex, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "a", "m",
			&regex, &error) == 0,
			SUITE, CASE_NAME, "multiline regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, text, 1,
			regex, &result, &error) == -1,
			SUITE, CASE_NAME, "expected multiline non-global rejection");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for multiline regex, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "a", "u",
			&regex, &error) == 0,
			SUITE, CASE_NAME, "unicode regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, text, 1,
			regex, &result, &error) == -1,
			SUITE, CASE_NAME, "expected unicode non-global rejection");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for unicode regex, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "a", "y",
			&regex, &error) == 0,
			SUITE, CASE_NAME, "sticky regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, text, 1,
			regex, &result, &error) == -1,
			SUITE, CASE_NAME, "expected sticky non-global rejection");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for sticky regex, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

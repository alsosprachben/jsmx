#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/replaceValue-call-skip-no-match"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t search;
	jsval_t result;
	jsmethod_error_t error;
	generated_replace_callback_record_t ctx = {
		.max_calls = 1,
		.should_throw = 1,
		.error_kind = JSMETHOD_ERROR_ABRUPT,
		.error_message = "callback should not run"
	};

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/replaceValue-call-skip-no-match.js
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a", 1, &text) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"b", 1, &search) == 0,
			SUITE, CASE_NAME, "search build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region, text,
			search, generated_record_replace_callback, &ctx, &result,
			&error) == 0, SUITE, CASE_NAME, "replaceAll no-match failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			"a", SUITE, CASE_NAME, "replaceAll no-match result") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected replaceAll no-match result");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"aa", 2, &search) == 0,
			SUITE, CASE_NAME, "second search build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region, text,
			search, generated_record_replace_callback, &ctx, &result,
			&error) == 0, SUITE, CASE_NAME,
			"replaceAll second no-match failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			"a", SUITE, CASE_NAME, "second replaceAll no-match result") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected second replaceAll no-match result");
	GENERATED_TEST_ASSERT(ctx.call_count == 0, SUITE, CASE_NAME,
			"expected callback to remain unevaluated, got %zu calls",
			ctx.call_count);
	return generated_test_pass(SUITE, CASE_NAME);
}

#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/replaceValue-call-matching-empty"

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
		.replacement_text = "abc"
	};

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/replaceValue-call-matching-empty.js
	 *
	 * The upstream sloppy-callback this observation is intentionally omitted.
	 * The translator-facing callback ABI models the observable call arguments
	 * and callback result, but not callback this binding.
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"", 0, &text) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"", 0, &search) == 0,
			SUITE, CASE_NAME, "search build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region, text,
			search, generated_record_replace_callback, &ctx, &result,
			&error) == 0, SUITE, CASE_NAME,
			"replaceAll empty-match callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			"abc", SUITE, CASE_NAME, "replaceAll empty-match result") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected replaceAll empty-match result");
	GENERATED_TEST_ASSERT(ctx.call_count == 1, SUITE, CASE_NAME,
			"expected one callback call, got %zu", ctx.call_count);
	GENERATED_TEST_ASSERT(strcmp(ctx.input, "") == 0, SUITE, CASE_NAME,
			"unexpected callback input");
	GENERATED_TEST_ASSERT(strcmp(ctx.matches[0], "") == 0, SUITE, CASE_NAME,
			"unexpected empty-string callback match");
	GENERATED_TEST_ASSERT(ctx.offsets[0] == 0, SUITE, CASE_NAME,
			"expected offset 0, got %zu", ctx.offsets[0]);
	return generated_test_pass(SUITE, CASE_NAME);
}

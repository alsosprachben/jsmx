#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/replaceValue-call-each-match-position"

int
main(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t text;
	jsval_t search;
	jsval_t result;
	jsmethod_error_t error;
	generated_replace_callback_record_t ctx = {
		.max_calls = 4,
		.replacement_text = "z"
	};

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/replaceValue-call-each-match-position.js
	 *
	 * The upstream sloppy-callback this observation is intentionally omitted.
	 * The translator-facing callback ABI models the observable call arguments
	 * and callback result, but not callback this binding.
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"ab c ab cdab cab c", 18, &text) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"ab c", 4, &search) == 0,
			SUITE, CASE_NAME, "search build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region, text,
			search, generated_record_replace_callback, &ctx, &result,
			&error) == 0, SUITE, CASE_NAME, "replaceAll callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			"z zdzz", SUITE, CASE_NAME, "replaceAll callback result") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected replaceAll callback result");
	GENERATED_TEST_ASSERT(ctx.call_count == 4, SUITE, CASE_NAME,
			"expected four callback calls, got %zu", ctx.call_count);
	GENERATED_TEST_ASSERT(strcmp(ctx.input, "ab c ab cdab cab c") == 0,
			SUITE, CASE_NAME, "unexpected callback input");
	for (size_t i = 0; i < 4; i++) {
		static const size_t expected_offsets[] = {0, 5, 10, 14};

		GENERATED_TEST_ASSERT(strcmp(ctx.matches[i], "ab c") == 0,
				SUITE, CASE_NAME, "unexpected callback match %zu", i);
		GENERATED_TEST_ASSERT(ctx.offsets[i] == expected_offsets[i],
				SUITE, CASE_NAME, "unexpected callback offset %zu: %zu",
				i, ctx.offsets[i]);
		GENERATED_TEST_ASSERT(ctx.capture_counts[i] == 0, SUITE, CASE_NAME,
				"expected zero captures for callback %zu, got %zu",
				i, ctx.capture_counts[i]);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

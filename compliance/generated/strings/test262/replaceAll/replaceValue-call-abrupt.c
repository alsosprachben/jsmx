#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/replaceValue-call-abrupt"

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
		.error_message = "Test262Error"
	};

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/replaceValue-call-abrupt.js
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a", 1, &text) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a", 1, &search) == 0,
			SUITE, CASE_NAME, "search build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region, text,
			search, generated_record_replace_callback, &ctx, &result,
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt callback failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT, SUITE,
			CASE_NAME, "expected ABRUPT callback failure, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(ctx.call_count == 1, SUITE, CASE_NAME,
			"expected one callback attempt, got %zu", ctx.call_count);
	return generated_test_pass(SUITE, CASE_NAME);
}

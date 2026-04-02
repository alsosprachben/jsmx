#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replace/S15.5.4.11_A1_T5"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t result;
	jsmethod_error_t error;
	generated_replace_callback_record_t ctx = {
		.max_calls = 1,
		.return_undefined = 1
	};

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replace/S15.5.4.11_A1_T5.js
	 *
	 * This translation lowers Function() to the observable callback result the
	 * upstream file relies on: returning undefined, which replace then
	 * stringifies to "undefined".
	 */

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"gnulluna", 8, &text) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, text,
			jsval_null(), generated_record_replace_callback, &ctx, &result,
			&error) == 0, SUITE, CASE_NAME, "replace callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region, result,
			"gundefineduna", SUITE, CASE_NAME, "callback replace result") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected callback replace result");
	GENERATED_TEST_ASSERT(ctx.call_count == 1, SUITE, CASE_NAME,
			"expected one callback call, got %zu", ctx.call_count);
	return generated_test_pass(SUITE, CASE_NAME);
}

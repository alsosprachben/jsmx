#include "compliance/generated/string_search_helpers.h"

#define SUITE_NAME "strings"
#define CASE_NAME "test262/search/S15.5.4.12_A1_T11"

int
main(void)
{
	generated_string_callback_ctx_t throw_ctx = {
		.should_throw = 1,
		.text = NULL,
		.len = 0,
		.calls_ptr = NULL,
	};
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Upstream source:
	 * test/built-ins/String/prototype/search/S15.5.4.12_A1_T11.js
	 */
	if (jsmethod_string_search_regex(&index,
			jsmethod_value_string_utf8((const uint8_t *)"ABBABABAB", 9),
			jsmethod_value_coercible(&throw_ctx,
					generated_string_callback_to_string),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_test_fail(SUITE_NAME, CASE_NAME,
				"#1: expected argument coercion to throw");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_test_fail(SUITE_NAME, CASE_NAME,
				"#1.1: expected ABRUPT, got %d", (int)error.kind);
	}
	return generated_test_pass(SUITE_NAME, CASE_NAME);
}

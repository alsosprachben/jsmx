#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charCodeAt/S15.5.4.5_A4"

int
main(void)
{
	generated_string_callback_ctx_t ctx = {1, NULL, 0};
	jsmethod_error_t error;
	double value;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/charCodeAt/S15.5.4.5_A4.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_coercible(&ctx, generated_string_callback_to_string),
			0, jsmethod_value_undefined(), &error) == -1, SUITE, CASE_NAME,
			"expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT, SUITE, CASE_NAME,
			"expected ABRUPT receiver coercion, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

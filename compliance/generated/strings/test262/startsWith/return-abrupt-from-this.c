#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/startsWith/return-abrupt-from-this"

int
main(void)
{
	generated_string_callback_ctx_t ctx = {1, NULL, 0};
	jsmethod_error_t error;
	int result;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/startsWith/return-abrupt-from-this.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_starts_with(&result,
			jsmethod_value_coercible(&ctx, generated_string_callback_to_string),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, jsmethod_value_undefined(), &error) == -1,
			SUITE, CASE_NAME, "expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT, SUITE, CASE_NAME,
			"expected ABRUPT receiver coercion, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

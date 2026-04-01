#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/separator-tostring-error"

int
main(void)
{
	generated_string_callback_ctx_t separator_ctx = {1, NULL, 0, NULL};
	jsmethod_error_t error;
	size_t count = 0;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/split/separator-tostring-error.js
	 *
	 * Idiomatic slow-path translation: the separator's throwing toString hook is
	 * modeled as an explicit coercion callback. This fixture also proves that
	 * split coerces the separator before the limit==0 early return.
	 */

	GENERATED_TEST_ASSERT(jsmethod_string_split(
			jsmethod_value_string_utf8((const uint8_t *)"foo", 3),
			1, jsmethod_value_coercible(&separator_ctx,
				generated_string_callback_to_string),
			1, jsmethod_value_number(0.0),
			NULL, NULL, &count, &error) == -1,
			SUITE, CASE_NAME,
			"expected separator coercion to fail before limit short-circuit");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT separator coercion");
	return generated_test_pass(SUITE, CASE_NAME);
}

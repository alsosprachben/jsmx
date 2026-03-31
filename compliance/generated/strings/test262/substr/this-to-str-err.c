#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/this-to-str-err"

int
main(void)
{
	generated_string_callback_ctx_t throw_ctx = {1, NULL, 0};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));

	/*
	 * Generated from test262:
	 * test/annexB/built-ins/String/prototype/substr/this-to-str-err.js
	 *
	 * This is an idiomatic slow-path translation. The upstream object receiver
	 * with a throwing toString hook is modeled as an explicit coercible callback
	 * that aborts before jsmethod substr runs.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_coercible(&throw_ctx,
				generated_string_callback_to_string),
			0, jsmethod_value_undefined(), 0, jsmethod_value_undefined(),
			&error) == -1, SUITE, CASE_NAME,
			"expected abrupt receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT receiver coercion, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/string_concat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/concat/S15.5.4.6_A4_T2"

int
main(void)
{
	static const uint16_t later_arg_text[] = {'A'};
	uint16_t storage[8];
	int later_arg_calls = 0;
	generated_string_callback_ctx_t receiver_ctx = {
		1, NULL, 0, NULL
	};
	generated_string_callback_ctx_t later_arg_ctx = {
		0, later_arg_text, GENERATED_LEN(later_arg_text), &later_arg_calls
	};
	jsmethod_value_t args[2];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/concat/S15.5.4.6_A4_T2.js
	 *
	 * This is an idiomatic slow-path translation. The receiver's throwing
	 * toString hook is modeled as an explicit coercion callback, and the later
	 * argument callback proves that later argument coercions are not evaluated
	 * once receiver coercion fails.
	 */

	args[0] = jsmethod_value_coercible(&later_arg_ctx,
			generated_string_callback_to_string);
	args[1] = jsmethod_value_undefined();
	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_concat(&out,
			jsmethod_value_coercible(&receiver_ctx,
				generated_string_callback_to_string), 2, args,
			&error) == -1, SUITE, CASE_NAME,
			"expected receiver coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT receiver coercion, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(later_arg_calls == 0, SUITE, CASE_NAME,
			"expected later args to remain unevaluated, got %d calls",
			later_arg_calls);
	return generated_test_pass(SUITE, CASE_NAME);
}

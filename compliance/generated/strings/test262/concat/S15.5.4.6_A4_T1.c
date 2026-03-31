#include <stdint.h>

#include "compliance/generated/string_concat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/concat/S15.5.4.6_A4_T1"

int
main(void)
{
	static const uint16_t receiver_text[] = {'o','n','e'};
	uint16_t storage[24];
	generated_string_callback_ctx_t receiver_ctx = {
		0, receiver_text, GENERATED_LEN(receiver_text), NULL
	};
	jsmethod_value_t args[2];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/concat/S15.5.4.6_A4_T1.js
	 *
	 * This is an idiomatic slow-path translation. The upstream receiver object
	 * with an overridden toString hook is lowered through an explicit coercion
	 * callback before concat dispatch.
	 */

	args[0] = jsmethod_value_string_utf8((const uint8_t *)"two", 3);
	args[1] = jsmethod_value_undefined();
	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_concat(&out,
			jsmethod_value_coercible(&receiver_ctx,
				generated_string_callback_to_string), 2, args,
			&error) == 0, SUITE, CASE_NAME,
			"failed receiver toString concat");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"onetwoundefined", SUITE, CASE_NAME,
			"__instance.concat(\"two\", x)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected receiver toString concat result");
	return generated_test_pass(SUITE, CASE_NAME);
}

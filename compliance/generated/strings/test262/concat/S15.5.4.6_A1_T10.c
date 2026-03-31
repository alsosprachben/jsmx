#include <stdint.h>

#include "compliance/generated/string_concat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/concat/S15.5.4.6_A1_T10"

int
main(void)
{
	static const uint16_t arg_a[] = {'A'};
	static const uint16_t arg_true[] = {'t','r','u','e'};
	static const uint16_t arg_42[] = {'4','2'};
	uint16_t storage[32];
	generated_string_callback_ctx_t arg_a_ctx = {
		0, arg_a, GENERATED_LEN(arg_a), NULL
	};
	generated_string_callback_ctx_t arg_true_ctx = {
		0, arg_true, GENERATED_LEN(arg_true), NULL
	};
	generated_string_callback_ctx_t arg_42_ctx = {
		0, arg_42, GENERATED_LEN(arg_42), NULL
	};
	jsmethod_value_t args[4];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/concat/S15.5.4.6_A1_T10.js
	 *
	 * This is an idiomatic slow-path translation. The upstream object
	 * arguments are lowered through explicit coercion callbacks that model
	 * translator-known ToString results outside jsmx.
	 */

	args[0] = jsmethod_value_coercible(&arg_a_ctx,
			generated_string_callback_to_string);
	args[1] = jsmethod_value_coercible(&arg_true_ctx,
			generated_string_callback_to_string);
	args[2] = jsmethod_value_coercible(&arg_42_ctx,
			generated_string_callback_to_string);
	args[3] = jsmethod_value_undefined();
	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsmethod_string_concat(&out,
			jsmethod_value_string_utf8((const uint8_t *)"lego", 4),
			4, args, &error) == 0, SUITE, CASE_NAME,
			"failed object-argument concat");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"legoAtrue42undefined", SUITE, CASE_NAME,
			"\"lego\".concat(__obj, __obj2, __obj3, x)")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected object-argument concat result");
	return generated_test_pass(SUITE, CASE_NAME);
}

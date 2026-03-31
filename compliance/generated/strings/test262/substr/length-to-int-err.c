#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/length-to-int-err"

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
	 * test/annexB/built-ins/String/prototype/substr/length-to-int-err.js
	 *
	 * This is an idiomatic slow-path translation. The upstream length object is
	 * lowered as an explicit coercion callback so the abrupt completion stays
	 * outside the flattened jsmethod layer.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_number(0.0), 1,
			jsmethod_value_coercible(&throw_ctx,
				generated_string_callback_to_string), &error) == -1,
			SUITE, CASE_NAME, "expected abrupt length coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_ABRUPT,
			SUITE, CASE_NAME, "expected ABRUPT length coercion, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_number(0.0), 1, jsmethod_value_symbol(),
			&error) == -1, SUITE, CASE_NAME,
			"expected Symbol length coercion to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE for Symbol length, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

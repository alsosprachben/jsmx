#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charCodeAt/S15.5.4.5_A3"

int
main(void)
{
	double value;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/charCodeAt/S15.5.4.5_A3.js
	 *
	 * This is an idiomatic slow-path translation. The upstream boxed String
	 * receiver is lowered to its primitive string value before dispatching into
	 * the thin jsmethod charCodeAt layer.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_string_utf8((const uint8_t *)"ABC", 3), 1,
			jsmethod_value_number(3.0), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-string charCodeAt(3)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_nan(value, SUITE, CASE_NAME,
			"charCodeAt(3)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"expected NaN from charCodeAt(3)");
	return generated_test_pass(SUITE, CASE_NAME);
}

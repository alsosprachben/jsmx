#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charCodeAt/S15.5.4.5_A1_T2"

int
main(void)
{
	double value;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/charCodeAt/S15.5.4.5_A1_T2.js
	 *
	 * This is an idiomatic slow-path translation. The upstream boxed Boolean
	 * receiver is lowered to its primitive ToString result "false" before
	 * dispatching into the thin jsmethod charCodeAt layer.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_bool(0), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-boolean charCodeAt(false)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_number(value, 0x66, SUITE,
			CASE_NAME, "charCodeAt(false)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charCodeAt(false)");
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_bool(1), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-boolean charCodeAt(true)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_number(value, 0x61, SUITE,
			CASE_NAME, "charCodeAt(true)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charCodeAt(true)");
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_number(2.0), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-boolean charCodeAt(true + 1)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_number(value, 0x6c, SUITE,
			CASE_NAME, "charCodeAt(true + 1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charCodeAt(true + 1)");
	return generated_test_pass(SUITE, CASE_NAME);
}

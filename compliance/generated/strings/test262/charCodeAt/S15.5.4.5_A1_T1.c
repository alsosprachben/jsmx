#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charCodeAt/S15.5.4.5_A1_T1"

int
main(void)
{
	double value;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/charCodeAt/S15.5.4.5_A1_T1.js
	 *
	 * This is an idiomatic slow-path translation. The upstream boxed Number
	 * receiver is lowered to its primitive ToString result "42" before
	 * dispatching into the thin jsmethod charCodeAt layer.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_string_utf8((const uint8_t *)"42", 2), 1,
			jsmethod_value_bool(0), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-number charCodeAt(false)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_number(value, 52.0, SUITE,
			CASE_NAME, "charCodeAt(false)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charCodeAt(false)");
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_string_utf8((const uint8_t *)"42", 2), 1,
			jsmethod_value_bool(1), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-number charCodeAt(true)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_number(value, 50.0, SUITE,
			CASE_NAME, "charCodeAt(true)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charCodeAt(true)");
	return generated_test_pass(SUITE, CASE_NAME);
}

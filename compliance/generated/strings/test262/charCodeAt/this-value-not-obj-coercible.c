#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charCodeAt/this-value-not-obj-coercible"

int
main(void)
{
	double value;
	jsmethod_error_t error;

	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_undefined(), 1, jsmethod_value_number(0.0),
			&error) == -1, SUITE, CASE_NAME,
			"expected undefined receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for undefined receiver, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
			jsmethod_value_null(), 1, jsmethod_value_number(0.0), &error) == -1,
			SUITE, CASE_NAME, "expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for null receiver, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

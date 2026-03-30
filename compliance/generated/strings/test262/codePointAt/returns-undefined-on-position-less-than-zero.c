#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/codePointAt/returns-undefined-on-position-less-than-zero"

int
main(void)
{
	jsmethod_error_t error;
	int has_value;
	double value;

	GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(-1.0), &error) == 0, SUITE, CASE_NAME,
			"failed codePointAt(-1)");
	GENERATED_TEST_ASSERT(has_value == 0, SUITE, CASE_NAME,
			"expected undefined for codePointAt(-1)");
	GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(-INFINITY), &error) == 0, SUITE, CASE_NAME,
			"failed codePointAt(-Infinity)");
	GENERATED_TEST_ASSERT(has_value == 0, SUITE, CASE_NAME,
			"expected undefined for codePointAt(-Infinity)");
	return generated_test_pass(SUITE, CASE_NAME);
}

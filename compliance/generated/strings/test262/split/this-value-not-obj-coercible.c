#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/this-value-not-obj-coercible"

int
main(void)
{
	jsmethod_error_t error;
	size_t count = 0;

	GENERATED_TEST_ASSERT(jsmethod_string_split(jsmethod_value_undefined(),
			1, jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, jsmethod_value_undefined(),
			NULL, NULL, &count, &error) == -1,
			SUITE, CASE_NAME, "expected undefined receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE for undefined receiver");

	GENERATED_TEST_ASSERT(jsmethod_string_split(jsmethod_value_null(),
			1, jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, jsmethod_value_undefined(),
			NULL, NULL, &count, &error) == -1,
			SUITE, CASE_NAME, "expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE,
			SUITE, CASE_NAME, "expected TYPE for null receiver");

	return generated_test_pass(SUITE, CASE_NAME);
}

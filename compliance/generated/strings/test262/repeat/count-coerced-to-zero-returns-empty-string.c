#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/count-coerced-to-zero-returns-empty-string"

static int
expect_empty_repeat(jsmethod_value_t count_value, const char *label)
{
	uint16_t buf[2];
	jsmethod_error_t error;
	jsstr16_t out;

	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, buf,
			GENERATED_LEN(buf),
			jsmethod_value_string_utf8((const uint8_t *)"ES2015", 6), 1,
			count_value, &error) == 0, SUITE, CASE_NAME,
			"%s: repeat failed", label);
	return generated_expect_empty_string(&out, SUITE, CASE_NAME, label);
}

int
main(void)
{
	GENERATED_TEST_ASSERT(expect_empty_repeat(jsmethod_value_number(NAN),
			"repeat(NaN)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(NaN)");
	GENERATED_TEST_ASSERT(expect_empty_repeat(jsmethod_value_null(),
			"repeat(null)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(null)");
	GENERATED_TEST_ASSERT(expect_empty_repeat(jsmethod_value_undefined(),
			"repeat(undefined)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(undefined)");
	GENERATED_TEST_ASSERT(expect_empty_repeat(jsmethod_value_bool(0),
			"repeat(false)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(false)");
	GENERATED_TEST_ASSERT(expect_empty_repeat(
			jsmethod_value_string_utf8((const uint8_t *)"0", 1),
			"repeat(\"0\")") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(\"0\")");
	GENERATED_TEST_ASSERT(expect_empty_repeat(jsmethod_value_number(0.9),
			"repeat(0.9)") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat(0.9)");
	return generated_test_pass(SUITE, CASE_NAME);
}

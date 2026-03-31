#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/empty-string-returns-empty"

static int
expect_empty(jsmethod_value_t count_value, const char *label)
{
	uint16_t buf[2];
	jsmethod_error_t error;
	jsstr16_t out;

	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, buf,
			GENERATED_LEN(buf),
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			count_value, &error) == 0, SUITE, CASE_NAME,
			"%s: repeat failed", label);
	return generated_expect_empty_string(&out, SUITE, CASE_NAME, label);
}

int
main(void)
{
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_number(1.0),
			"repeat(1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected repeat(1)");
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_number(3.0),
			"repeat(3)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected repeat(3)");
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_number(2147483647.0),
			"repeat(maxSafe32bitInt)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected repeat(maxSafe32bitInt)");
	return generated_test_pass(SUITE, CASE_NAME);
}

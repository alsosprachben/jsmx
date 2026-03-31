#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/count-is-zero-returns-empty-string"

int
main(void)
{
	uint16_t buf[2];
	jsmethod_error_t error;
	jsstr16_t out;

	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, buf,
			GENERATED_LEN(buf),
			jsmethod_value_string_utf8((const uint8_t *)"foo", 3), 1,
			jsmethod_value_number(0.0), &error) == 0,
			SUITE, CASE_NAME, "repeat(0) failed");
	GENERATED_TEST_ASSERT(generated_expect_empty_string(&out, SUITE, CASE_NAME,
			"repeat(0)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected repeat(0)");
	return generated_test_pass(SUITE, CASE_NAME);
}

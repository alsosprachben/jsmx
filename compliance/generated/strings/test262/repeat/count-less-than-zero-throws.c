#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/count-less-than-zero-throws"

static int
expect_range(jsmethod_value_t count_value, const char *label)
{
	uint16_t buf[2];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	if (generated_measure_and_repeat(&out, buf, GENERATED_LEN(buf),
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			count_value, &error) != -1) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected repeat to fail", label);
	}
	if (error.kind != JSMETHOD_ERROR_RANGE) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected RANGE, got %d", label,
				(int)error.kind);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	GENERATED_TEST_ASSERT(expect_range(jsmethod_value_number(-1.0),
			"repeat(-1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected repeat(-1)");
	GENERATED_TEST_ASSERT(expect_range(jsmethod_value_number(-INFINITY),
			"repeat(-Infinity)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected repeat(-Infinity)");
	return generated_test_pass(SUITE, CASE_NAME);
}

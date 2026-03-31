#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/repeat/count-is-infinity-throws"

int
main(void)
{
	uint16_t buf[2];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(generated_measure_and_repeat(&out, buf,
			GENERATED_LEN(buf),
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_number(INFINITY), &error) == -1,
			SUITE, CASE_NAME, "expected repeat(Infinity) to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_RANGE, SUITE, CASE_NAME,
			"expected RANGE for repeat(Infinity), got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

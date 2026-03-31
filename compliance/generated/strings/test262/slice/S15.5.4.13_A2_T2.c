#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/slice/S15.5.4.13_A2_T2"

int
main(void)
{
	static const uint16_t expected[] = {
		0x0074, 0x0068, 0x0069, 0x0073, 0x0020, 0x0069, 0x0073, 0x0020,
		0x0061, 0x0020, 0x0073, 0x0074, 0x0072, 0x0069, 0x006E, 0x0067,
		0x0020, 0x006F, 0x0062, 0x006A, 0x0065, 0x0063, 0x0074
	};
	uint16_t buf[32];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_slice(&out,
			jsmethod_value_string_utf8(
					(const uint8_t *)"this is a string object", 23),
			1, jsmethod_value_number(NAN), 1,
			jsmethod_value_number(INFINITY), &error) == 0,
			SUITE, CASE_NAME, "failed slice(NaN, Infinity)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected, 23,
			SUITE, CASE_NAME, "slice(NaN, Infinity)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected slice(NaN, Infinity)");
	return generated_test_pass(SUITE, CASE_NAME);
}

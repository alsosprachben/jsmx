#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/slice/S15.5.4.13_A2_T7"

int
main(void)
{
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_slice(&out,
			jsmethod_value_string_utf8(
					(const uint8_t *)"this is a string object", 23),
			1, jsmethod_value_number(23.0), 1, jsmethod_value_number(23.0),
			&error) == 0, SUITE, CASE_NAME, "failed slice(len, len)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, NULL, 0,
			SUITE, CASE_NAME, "slice(len, len)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected slice(len, len)");
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substring/S15.5.4.15_A2_T10"

int
main(void)
{
	static const uint16_t expected[] = {
		0x0074, 0x0068, 0x0069, 0x0073, 0x005F, 0x0069, 0x0073, 0x005F
	};
	uint16_t buf[16];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_substring(&out,
			jsmethod_value_string_utf8(
					(const uint8_t *)"this_is_a_string object", 23),
			1, jsmethod_value_number(0.0), 1, jsmethod_value_number(8.0),
			&error) == 0, SUITE, CASE_NAME, "failed substring(0, 8)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected, 8,
			SUITE, CASE_NAME, "substring(0, 8)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substring(0, 8)");
	return generated_test_pass(SUITE, CASE_NAME);
}

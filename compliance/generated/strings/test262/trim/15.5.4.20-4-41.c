#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/trim/15.5.4.20-4-41"

int
main(void)
{
	static const uint16_t input[] = {'a', 'b', 0x200B, 'c'};
	uint16_t buf[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_trim(&out,
			jsmethod_value_string_utf16(input, GENERATED_LEN(input)),
			&error) == 0, SUITE, CASE_NAME, "failed trim");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, input,
			GENERATED_LEN(input), SUITE, CASE_NAME,
			"trim result") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected trim result");
	return generated_test_pass(SUITE, CASE_NAME);
}

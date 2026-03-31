#include <stdint.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/trimStart/this-value-line-terminator"

int
main(void)
{
	uint16_t str[GENERATED_LEN(generated_trim_line_terminators) * 3 + 2];
	uint16_t expected[GENERATED_LEN(generated_trim_line_terminators) * 2 + 2];
	uint16_t buf[GENERATED_LEN(str)];
	size_t n = GENERATED_LEN(generated_trim_line_terminators);
	jsmethod_error_t error;
	jsstr16_t out;

	memcpy(str, generated_trim_line_terminators,
			sizeof(generated_trim_line_terminators));
	str[n] = 'a';
	memcpy(str + n + 1, generated_trim_line_terminators,
			sizeof(generated_trim_line_terminators));
	str[(n * 2) + 1] = 'b';
	memcpy(str + (n * 2) + 2, generated_trim_line_terminators,
			sizeof(generated_trim_line_terminators));

	expected[0] = 'a';
	memcpy(expected + 1, generated_trim_line_terminators,
			sizeof(generated_trim_line_terminators));
	expected[n + 1] = 'b';
	memcpy(expected + n + 2, generated_trim_line_terminators,
			sizeof(generated_trim_line_terminators));

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_trim_start(&out,
			jsmethod_value_string_utf16(str, GENERATED_LEN(str)),
			&error) == 0, SUITE, CASE_NAME, "failed trimStart");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected,
			GENERATED_LEN(expected), SUITE, CASE_NAME,
			"trimStart result") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected trimStart result");
	return generated_test_pass(SUITE, CASE_NAME);
}

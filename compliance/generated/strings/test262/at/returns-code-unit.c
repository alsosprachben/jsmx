#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/at/returns-code-unit"

typedef struct at_code_unit_case_s {
	double index;
	uint16_t expected;
} at_code_unit_case_t;

int
main(void)
{
	static const at_code_unit_case_t cases[] = {
		{0.0, 0x0031},
		{1.0, 0x0032},
		{2.0, 0xD800},
		{3.0, 0x0033},
		{4.0, 0x0034},
	};
	static const uint16_t input[] = {0x0031, 0x0032, 0xD800, 0x0033, 0x0034};
	uint16_t in_buf[16];
	uint16_t out_buf[4];
	jsmethod_error_t error;
	jsstr16_t in;
	jsstr16_t out;
	int has_value;
	size_t i;

	jsstr16_init_from_buf(&in, (char *)in_buf, sizeof(in_buf));
	jsstr16_init_from_buf(&out, (char *)out_buf, sizeof(out_buf));
	GENERATED_TEST_ASSERT(jsstr16_set_from_utf16(&in, input,
			sizeof(input) / sizeof(input[0])) ==
			sizeof(input) / sizeof(input[0]), SUITE, CASE_NAME,
			"failed to build UTF-16 input");
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
				jsmethod_value_string_utf16(in.codeunits,
					jsstr16_get_utf16len(&in)), 1,
				jsmethod_value_number(cases[i].index), &error) == 0,
				SUITE, CASE_NAME, "failed at case %zu", i);
		GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
				"expected hit for case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
				&cases[i].expected, 1, SUITE, CASE_NAME,
				"code-unit preserving at") == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "unexpected code-unit case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

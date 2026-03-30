#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/codePointAt/return-first-code-unit"

typedef struct first_code_unit_case_s {
	uint16_t first;
	uint16_t second;
	double expected;
} first_code_unit_case_t;

int
main(void)
{
	static const first_code_unit_case_t cases[] = {
		{0xD800, 0xDBFF, 0xD800},
		{0xD800, 0xE000, 0xD800},
		{0xDAAA, 0xDBFF, 0xDAAA},
		{0xDAAA, 0xE000, 0xDAAA},
		{0xDBFF, 0xDBFF, 0xDBFF},
		{0xDBFF, 0xE000, 0xDBFF},
		{0xD800, 0x0000, 0xD800},
		{0xD800, 0xFFFF, 0xD800},
		{0xDAAA, 0x0000, 0xDAAA},
		{0xDAAA, 0xFFFF, 0xDAAA},
		{0xDBFF, 0xDBFF, 0xDBFF},
		{0xDBFF, 0xFFFF, 0xDBFF},
	};
	uint16_t in_buf[16];
	jsmethod_error_t error;
	jsstr16_t in;
	int has_value;
	double value;
	size_t i;

	jsstr16_init_from_buf(&in, (char *)in_buf, sizeof(in_buf));
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		uint16_t input[2] = {cases[i].first, cases[i].second};
		GENERATED_TEST_ASSERT(jsstr16_set_from_utf16(&in, input, 2) == 2,
				SUITE, CASE_NAME, "failed to build input %zu", i);
		GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value,
				jsmethod_value_string_utf16(in.codeunits,
					jsstr16_get_utf16len(&in)), 1,
				jsmethod_value_number(0.0),
				&error) == 0, SUITE, CASE_NAME,
				"failed codePointAt case %zu", i);
		GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
				"expected hit for case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_number(value,
				cases[i].expected, SUITE, CASE_NAME,
				"lead-surrogate first code unit") == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "unexpected first code unit case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

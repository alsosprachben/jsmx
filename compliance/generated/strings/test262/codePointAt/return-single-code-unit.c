#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/codePointAt/return-single-code-unit"

typedef struct single_code_unit_case_s {
	const uint16_t *input;
	size_t len;
	double index;
	double expected;
} single_code_unit_case_t;

int
main(void)
{
	static const uint16_t ascii[] = {0x0061, 0x0062, 0x0063};
	static const uint16_t aaaa_bbbb[] = {0xAAAA, 0xBBBB};
	static const uint16_t d7ff_aaaa[] = {0xD7FF, 0xAAAA};
	static const uint16_t dc00_aaaa[] = {0xDC00, 0xAAAA};
	static const uint16_t end_d800[] = {0x0031, 0x0032, 0x0033, 0xD800};
	static const uint16_t end_daaa[] = {0x0031, 0x0032, 0x0033, 0xDAAA};
	static const uint16_t end_dbff[] = {0x0031, 0x0032, 0x0033, 0xDBFF};
	static const single_code_unit_case_t cases[] = {
		{ascii, 3, 0.0, 97.0},
		{ascii, 3, 1.0, 98.0},
		{ascii, 3, 2.0, 99.0},
		{aaaa_bbbb, 2, 0.0, 0xAAAA},
		{d7ff_aaaa, 2, 0.0, 0xD7FF},
		{dc00_aaaa, 2, 0.0, 0xDC00},
		{aaaa_bbbb, 2, 0.0, 0xAAAA},
		{end_d800, 4, 3.0, 0xD800},
		{end_daaa, 4, 3.0, 0xDAAA},
		{end_dbff, 4, 3.0, 0xDBFF},
	};
	uint16_t in_buf[16];
	jsmethod_error_t error;
	jsstr16_t in;
	int has_value;
	double value;
	size_t i;

	jsstr16_init_from_buf(&in, (char *)in_buf, sizeof(in_buf));
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		GENERATED_TEST_ASSERT(jsstr16_set_from_utf16(&in, cases[i].input,
				cases[i].len) == cases[i].len, SUITE, CASE_NAME,
				"failed to build input %zu", i);
		GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value,
				jsmethod_value_string_utf16(in.codeunits,
					jsstr16_get_utf16len(&in)), 1,
				jsmethod_value_number(cases[i].index), &error) == 0,
				SUITE, CASE_NAME, "failed codePointAt case %zu", i);
		GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
				"expected hit for case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_number(value,
				cases[i].expected, SUITE, CASE_NAME, "single code unit") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected single code unit case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

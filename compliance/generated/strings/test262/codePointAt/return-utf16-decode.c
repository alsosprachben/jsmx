#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/codePointAt/return-utf16-decode"

typedef struct code_point_case_s {
	uint16_t lead;
	uint16_t trail;
	double expected;
} code_point_case_t;

int
main(void)
{
	static const code_point_case_t cases[] = {
		{0xD800, 0xDC00, 65536.0},
		{0xD800, 0xDDD0, 66000.0},
		{0xD800, 0xDFFF, 66559.0},
		{0xDAAA, 0xDC00, 763904.0},
		{0xDAAA, 0xDDD0, 764368.0},
		{0xDAAA, 0xDFFF, 764927.0},
		{0xDBFF, 0xDC00, 1113088.0},
		{0xDBFF, 0xDDD0, 1113552.0},
		{0xDBFF, 0xDFFF, 1114111.0},
	};
	uint16_t in_buf[16];
	jsmethod_error_t error;
	jsstr16_t in;
	int has_value;
	double value;
	size_t i;

	jsstr16_init_from_buf(&in, (char *)in_buf, sizeof(in_buf));
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		uint16_t input[2] = {cases[i].lead, cases[i].trail};
		GENERATED_TEST_ASSERT(jsstr16_set_from_utf16(&in, input, 2) == 2,
				SUITE, CASE_NAME, "failed to build surrogate pair %zu", i);
		GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value,
				jsmethod_value_string_utf16(in.codeunits,
					jsstr16_get_utf16len(&in)), 1,
				jsmethod_value_number(0.0),
				&error) == 0, SUITE, CASE_NAME,
				"failed codePointAt case %zu", i);
		GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
				"expected hit for case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_number(value,
				cases[i].expected, SUITE, CASE_NAME, "codePointAt decode") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected codePointAt case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charAt/pos-rounding"

typedef struct char_at_round_case_s {
	double position;
	uint16_t expected;
} char_at_round_case_t;

int
main(void)
{
	static const char_at_round_case_t cases[] = {
		{-0.99999, 0x0061},
		{-0.00001, 0x0061},
		{0.00001, 0x0061},
		{0.99999, 0x0061},
		{1.00001, 0x0062},
		{1.99999, 0x0062},
	};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;
	size_t i;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_char_at(&out,
				jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
				jsmethod_value_number(cases[i].position), &error) == 0,
				SUITE, CASE_NAME, "failed charAt case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
				&cases[i].expected, 1, SUITE, CASE_NAME, "rounded charAt") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected rounded charAt case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charCodeAt/pos-rounding"

typedef struct char_code_case_s {
	double position;
	double expected;
} char_code_case_t;

int
main(void)
{
	static const char_code_case_t cases[] = {
		{-0.99999, 97.0},
		{-0.00001, 97.0},
		{0.00001, 97.0},
		{0.99999, 97.0},
		{1.00001, 98.0},
		{1.99999, 98.0},
	};
	double value;
	jsmethod_error_t error;
	size_t i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_char_code_at(&value,
				jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
				jsmethod_value_number(cases[i].position), &error) == 0,
				SUITE, CASE_NAME, "failed charCodeAt case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_number(value,
				cases[i].expected, SUITE, CASE_NAME, "rounded charCodeAt") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected rounded charCodeAt case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

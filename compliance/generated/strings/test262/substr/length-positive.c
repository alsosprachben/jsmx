#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/length-positive"

typedef struct substr_case_s {
	double start;
	double length;
	const uint16_t *expected;
	size_t expected_len;
	const char *label;
} substr_case_t;

int
main(void)
{
	static const uint16_t a[] = {'a'};
	static const uint16_t ab[] = {'a', 'b'};
	static const uint16_t abc[] = {'a', 'b', 'c'};
	static const uint16_t b[] = {'b'};
	static const uint16_t bc[] = {'b', 'c'};
	static const uint16_t c[] = {'c'};
	static const substr_case_t cases[] = {
		{0.0, 1.0, a, 1, "substr(0, 1)"},
		{0.0, 2.0, ab, 2, "substr(0, 2)"},
		{0.0, 3.0, abc, 3, "substr(0, 3)"},
		{0.0, 4.0, abc, 3, "substr(0, 4)"},
		{1.0, 1.0, b, 1, "substr(1, 1)"},
		{1.0, 2.0, bc, 2, "substr(1, 2)"},
		{1.0, 3.0, bc, 2, "substr(1, 3)"},
		{2.0, 1.0, c, 1, "substr(2, 1)"},
		{2.0, 4.0, c, 1, "substr(2, 4)"},
		{3.0, 1.0, NULL, 0, "substr(3, 1)"}
	};
	uint16_t buf[8];
	jsmethod_error_t error;
	jsstr16_t out;
	size_t i;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
				jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
				jsmethod_value_number(cases[i].start), 1,
				jsmethod_value_number(cases[i].length), &error) == 0,
				SUITE, CASE_NAME, "%s: substr call failed", cases[i].label);
		GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
				cases[i].expected, cases[i].expected_len, SUITE, CASE_NAME,
				cases[i].label) == GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "%s: unexpected result", cases[i].label);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

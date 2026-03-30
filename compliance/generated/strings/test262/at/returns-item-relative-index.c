#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/at/returns-item-relative-index"

typedef struct at_case_s {
	double index;
	uint16_t expected;
} at_case_t;

int
main(void)
{
	static const at_case_t cases[] = {
		{0.0, 0x0031},
		{-1.0, 0x0035},
		{-3.0, 0x0033},
		{-4.0, 0x0032},
	};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;
	int has_value;
	size_t i;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
				jsmethod_value_string_utf8((const uint8_t *)"12345", 5), 1,
				jsmethod_value_number(cases[i].index), &error) == 0,
				SUITE, CASE_NAME, "failed at case %zu", i);
		GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
				"expected hit for case %zu", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
				&cases[i].expected, 1, SUITE, CASE_NAME, "relative at") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected relative at case %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

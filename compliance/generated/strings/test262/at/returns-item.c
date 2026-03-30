#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/at/returns-item"

int
main(void)
{
	static const uint16_t expected[] = {0x0031, 0x0032, 0x0033, 0x0034, 0x0035};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;
	int has_value;
	size_t i;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
				jsmethod_value_string_utf8((const uint8_t *)"12345", 5), 1,
				jsmethod_value_number((double)i), &error) == 0,
				SUITE, CASE_NAME, "failed at(%zu)", i);
		GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
				"expected hit for at(%zu)", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
				&expected[i], 1, SUITE, CASE_NAME, "at(index)") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected at(%zu) result", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

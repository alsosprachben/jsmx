#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/at/returns-undefined-for-out-of-range-index"

int
main(void)
{
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;
	int has_value;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_number(-2.0), &error) == 0, SUITE, CASE_NAME,
			"failed at(-2)");
	GENERATED_TEST_ASSERT(has_value == 0, SUITE, CASE_NAME,
			"expected undefined for at(-2)");
	GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_number(0.0), &error) == 0, SUITE, CASE_NAME,
			"failed at(0)");
	GENERATED_TEST_ASSERT(has_value == 0, SUITE, CASE_NAME,
			"expected undefined for at(0)");
	GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
			jsmethod_value_string_utf8((const uint8_t *)"", 0), 1,
			jsmethod_value_number(1.0), &error) == 0, SUITE, CASE_NAME,
			"failed at(1)");
	GENERATED_TEST_ASSERT(has_value == 0, SUITE, CASE_NAME,
			"expected undefined for at(1)");
	return generated_test_pass(SUITE, CASE_NAME);
}

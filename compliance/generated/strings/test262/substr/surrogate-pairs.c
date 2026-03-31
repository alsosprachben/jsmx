#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/surrogate-pairs"

int
main(void)
{
	static const uint16_t pair[] = {0xD834, 0xDF06};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf16(pair, 2), 1,
			jsmethod_value_number(0.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(0)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, pair, 2,
			SUITE, CASE_NAME, "substr(0)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(0)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf16(pair, 2), 1,
			jsmethod_value_number(1.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(1)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, pair + 1, 1,
			SUITE, CASE_NAME, "substr(1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(1)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf16(pair, 2), 1,
			jsmethod_value_number(0.0), 1, jsmethod_value_number(1.0),
			&error) == 0, SUITE, CASE_NAME, "failed substr(0, 1)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, pair, 1,
			SUITE, CASE_NAME, "substr(0, 1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(0, 1)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf16(pair, 2), 1,
			jsmethod_value_number(2.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(2)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, NULL, 0,
			SUITE, CASE_NAME, "substr(2)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(2)");
	return generated_test_pass(SUITE, CASE_NAME);
}

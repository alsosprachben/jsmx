#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/length-undef"

int
main(void)
{
	uint16_t buf[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(0.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(0)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
			(const uint16_t[]){'a', 'b', 'c'}, 3, SUITE, CASE_NAME,
			"substr(0)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(0)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(1.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(1)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
			(const uint16_t[]){'b', 'c'}, 2, SUITE, CASE_NAME,
			"substr(1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(1)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(2.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(2)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
			(const uint16_t[]){'c'}, 1, SUITE, CASE_NAME,
			"substr(2)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(2)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(3.0), 0, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME, "failed substr(3)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, NULL, 0,
			SUITE, CASE_NAME, "substr(3)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(3)");

	GENERATED_TEST_ASSERT(jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(1.0), 1, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME,
			"failed substr(1, undefined)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out,
			(const uint16_t[]){'b', 'c'}, 2, SUITE, CASE_NAME,
			"substr(1, undefined)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(1, undefined)");
	return generated_test_pass(SUITE, CASE_NAME);
}

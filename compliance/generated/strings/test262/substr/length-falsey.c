#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substr/length-falsey"

static int
expect_empty(jsmethod_value_t length_value, const char *label)
{
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	if (jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(1.0), 1, length_value, &error) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: substr call failed", label);
	}
	return generated_expect_accessor_string(&out, NULL, 0, SUITE, CASE_NAME,
			label);
}

int
main(void)
{
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_bool(0),
			"substr(1, false)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(1, false)");
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_number(NAN),
			"substr(1, NaN)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(1, NaN)");
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_string_utf8(
			(const uint8_t *)"", 0), "substr(1, \"\")") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected substr(1, \"\")");
	GENERATED_TEST_ASSERT(expect_empty(jsmethod_value_null(),
			"substr(1, null)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected substr(1, null)");
	return generated_test_pass(SUITE, CASE_NAME);
}

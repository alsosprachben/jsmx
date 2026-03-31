#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substring/S15.5.4.15_A3_T2"

int
main(void)
{
	static const uint16_t expected[] = {
		0x0031, 0x002C, 0x0032, 0x002C, 0x0033, 0x002C, 0x0034, 0x002C,
		0x0035
	};
	uint16_t buf[16];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/substring/S15.5.4.15_A3_T2.js
	 *
	 * This is an idiomatic slow-path translation. The upstream Array receiver
	 * is lowered through its known primitive ToString result "1,2,3,4,5"
	 * before dispatching into the thin jsmethod substring layer.
	 */
	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_substring(&out,
			jsmethod_value_string_utf8((const uint8_t *)"1,2,3,4,5", 9), 1,
			jsmethod_value_number(9.0), 1, jsmethod_value_number(-INFINITY),
			&error) == 0, SUITE, CASE_NAME,
			"failed lowered substring(9, -Infinity)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected, 9,
			SUITE, CASE_NAME, "lowered substring(9, -Infinity)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected lowered substring(9, -Infinity)");
	return generated_test_pass(SUITE, CASE_NAME);
}

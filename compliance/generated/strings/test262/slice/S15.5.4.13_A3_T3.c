#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/slice/S15.5.4.13_A3_T3"

int
main(void)
{
	static const uint16_t expected[] = {
		0x0075, 0x006E, 0x0063, 0x0074, 0x0069, 0x006F, 0x006E
	};
	uint16_t first_buf[16];
	uint16_t second_buf[16];
	jsmethod_error_t error;
	jsstr16_t first;
	jsstr16_t second;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/slice/S15.5.4.13_A3_T3.js
	 *
	 * This is an idiomatic slow-path translation. The upstream object receiver
	 * is lowered through its known primitive ToString result "function(){}"
	 * before dispatching into nested jsmethod slice calls.
	 */
	jsstr16_init_from_buf(&first, (char *)first_buf, sizeof(first_buf));
	jsstr16_init_from_buf(&second, (char *)second_buf, sizeof(second_buf));
	GENERATED_TEST_ASSERT(jsmethod_string_slice(&first,
			jsmethod_value_string_utf8((const uint8_t *)"function(){}", 12),
			1, jsmethod_value_number(-INFINITY), 1,
			jsmethod_value_number(8.0), &error) == 0,
			SUITE, CASE_NAME, "failed lowered first slice");
	GENERATED_TEST_ASSERT(jsmethod_string_slice(&second,
			jsmethod_value_string_utf16(first.codeunits, first.len), 1,
			jsmethod_value_number(1.0), 1,
			jsmethod_value_number(INFINITY), &error) == 0,
			SUITE, CASE_NAME, "failed lowered second slice");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&second, expected, 7,
			SUITE, CASE_NAME, "nested lowered slice") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected nested lowered slice");
	return generated_test_pass(SUITE, CASE_NAME);
}

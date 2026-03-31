#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/slice/S15.5.4.13_A3_T1"

int
main(void)
{
	static const uint16_t expected[] = {
		0x005B, 0x006F, 0x0062, 0x006A, 0x0065, 0x0063, 0x0074, 0x0020
	};
	uint16_t buf[16];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/slice/S15.5.4.13_A3_T1.js
	 *
	 * This is an idiomatic slow-path translation. The upstream Object
	 * receiver is lowered through its known primitive ToString result
	 * "[object Object]" before dispatching into the thin jsmethod slice layer.
	 */
	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_slice(&out,
			jsmethod_value_string_utf8((const uint8_t *)"[object Object]", 15),
			1, jsmethod_value_number(0.0), 1, jsmethod_value_number(8.0),
			&error) == 0, SUITE, CASE_NAME, "failed lowered slice(0, 8)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected, 8,
			SUITE, CASE_NAME, "lowered slice(0, 8)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected lowered slice(0, 8)");
	return generated_test_pass(SUITE, CASE_NAME);
}

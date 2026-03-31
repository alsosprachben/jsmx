#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/substring/S15.5.4.15_A1_T2"

int
main(void)
{
	static const uint16_t expected[] = {0x0061, 0x006C, 0x0073, 0x0065};
	uint16_t buf[8];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/substring/S15.5.4.15_A1_T2.js
	 *
	 * This is an idiomatic slow-path translation. The upstream Boolean
	 * receiver is lowered through its known primitive ToString result "false"
	 * before dispatching into the thin jsmethod substring layer.
	 */
	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_substring(&out,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_number(1.0), 1, jsmethod_value_undefined(),
			&error) == 0, SUITE, CASE_NAME,
			"failed lowered substring(true, undefined)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected, 4,
			SUITE, CASE_NAME, "lowered substring(true, undefined)") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected lowered substring(true, undefined)");
	return generated_test_pass(SUITE, CASE_NAME);
}

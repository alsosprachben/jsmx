#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charAt/S15.5.4.4_A4_T3"

int
main(void)
{
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;
	int i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/charAt/S15.5.4.4_A4_T3.js
	 */
	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	for (i = 6; i < 8; i++) {
		GENERATED_TEST_ASSERT(jsmethod_string_char_at(&out,
				jsmethod_value_string_utf8((const uint8_t *)"ABCABC", 6), 1,
				jsmethod_value_number((double)i), &error) == 0,
				SUITE, CASE_NAME, "failed charAt(%d)", i);
		GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, NULL, 0,
				SUITE, CASE_NAME, "out-of-range index yields empty string") ==
				GENERATED_TEST_PASS, SUITE, CASE_NAME,
				"unexpected charAt(%d) result", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/charAt/S15.5.4.4_A1_T2"

int
main(void)
{
	static const uint16_t expect0[] = {0x0066};
	static const uint16_t expect1[] = {0x0061};
	static const uint16_t expect2[] = {0x006c};
	uint16_t buf0[4];
	uint16_t buf1[4];
	uint16_t buf2[4];
	jsmethod_error_t error;
	jsstr16_t out0;
	jsstr16_t out1;
	jsstr16_t out2;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/charAt/S15.5.4.4_A1_T2.js
	 *
	 * This is an idiomatic slow-path translation. The upstream boxed Boolean
	 * receiver is lowered to its primitive ToString result "false" before
	 * dispatching into the thin jsmethod charAt layer.
	 */
	jsstr16_init_from_buf(&out0, (char *)buf0, sizeof(buf0));
	jsstr16_init_from_buf(&out1, (char *)buf1, sizeof(buf1));
	jsstr16_init_from_buf(&out2, (char *)buf2, sizeof(buf2));
	GENERATED_TEST_ASSERT(jsmethod_string_char_at(&out0,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_bool(0), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-boolean charAt(false)");
	GENERATED_TEST_ASSERT(jsmethod_string_char_at(&out1,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_bool(1), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-boolean charAt(true)");
	GENERATED_TEST_ASSERT(jsmethod_string_char_at(&out2,
			jsmethod_value_string_utf8((const uint8_t *)"false", 5), 1,
			jsmethod_value_number(2.0), &error) == 0, SUITE, CASE_NAME,
			"failed boxed-boolean charAt(true + 1)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out0, expect0, 1,
			SUITE, CASE_NAME, "charAt(false)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charAt(false)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out1, expect1, 1,
			SUITE, CASE_NAME, "charAt(true)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charAt(true)");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out2, expect2, 1,
			SUITE, CASE_NAME, "charAt(true + 1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected charAt(true + 1)");
	return generated_test_pass(SUITE, CASE_NAME);
}

#include <math.h>
#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/codePointAt/return-code-unit-coerced-position"

int
main(void)
{
	static const uint16_t astral[] = {0xD800, 0xDC00};
	uint16_t in_buf[8];
	jsmethod_error_t error;
	jsstr16_t in;
	int has_value;
	double value;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/codePointAt/return-code-unit-coerced-position.js
	 *
	 * This is an idiomatic slow-path translation. The upstream array positions
	 * `[]` and `[1]` are lowered through translator-known primitive ToString
	 * results "" and "1" before dispatching into the thin jsmethod
	 * codePointAt layer.
	 */
	jsstr16_init_from_buf(&in, (char *)in_buf, sizeof(in_buf));
	GENERATED_TEST_ASSERT(jsstr16_set_from_utf16(&in, astral, 2) == 2,
			SUITE, CASE_NAME, "failed to build astral input");

#define CHECK_POS(POS_VALUE, EXPECTED, LABEL) do { \
	GENERATED_TEST_ASSERT(jsmethod_string_code_point_at(&has_value, &value, \
			jsmethod_value_string_utf16(in.codeunits, \
				jsstr16_get_utf16len(&in)), 1, (POS_VALUE), &error) == 0, \
			SUITE, CASE_NAME, "failed %s", (LABEL)); \
	GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME, \
			"expected hit for %s", (LABEL)); \
	GENERATED_TEST_ASSERT(generated_expect_accessor_number(value, (EXPECTED), \
			SUITE, CASE_NAME, (LABEL)) == GENERATED_TEST_PASS, \
			SUITE, CASE_NAME, "unexpected %s", (LABEL)); \
} while (0)

	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"", 0), 65536.0,
			"codePointAt(\"\")");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"0", 1), 65536.0,
			"codePointAt(\"0\")");
	CHECK_POS(jsmethod_value_number(NAN), 65536.0, "codePointAt(NaN)");
	CHECK_POS(jsmethod_value_bool(0), 65536.0, "codePointAt(false)");
	CHECK_POS(jsmethod_value_null(), 65536.0, "codePointAt(null)");
	CHECK_POS(jsmethod_value_undefined(), 65536.0, "codePointAt(undefined)");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"", 0), 65536.0,
			"codePointAt([])");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"1", 1), 56320.0,
			"codePointAt(\"1\")");
	CHECK_POS(jsmethod_value_bool(1), 56320.0, "codePointAt(true)");
	CHECK_POS(jsmethod_value_string_utf8((const uint8_t *)"1", 1), 56320.0,
			"codePointAt([1])");

#undef CHECK_POS
	return generated_test_pass(SUITE, CASE_NAME);
}

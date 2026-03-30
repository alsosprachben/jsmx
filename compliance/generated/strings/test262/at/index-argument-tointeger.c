#include <stdint.h>

#include "compliance/generated/string_accessor_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/at/index-argument-tointeger"

int
main(void)
{
	static const uint16_t expected[] = {0x0031};
	uint16_t buf[4];
	jsmethod_error_t error;
	jsstr16_t out;
	int has_value;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/at/index-argument-tointeger.js
	 *
	 * This is an idiomatic slow-path translation. The upstream object-literal
	 * index value is lowered through the translator-known primitive valueOf
	 * result 1 before dispatching into the thin jsmethod at layer. The
	 * prototype-object typeof assertion is intentionally omitted.
	 */
	jsstr16_init_from_buf(&out, (char *)buf, sizeof(buf));
	GENERATED_TEST_ASSERT(jsmethod_string_at(&out, &has_value,
			jsmethod_value_string_utf8((const uint8_t *)"01", 2), 1,
			jsmethod_value_number(1.0), &error) == 0, SUITE, CASE_NAME,
			"failed lowered at(index)");
	GENERATED_TEST_ASSERT(has_value == 1, SUITE, CASE_NAME,
			"expected lowered index to hit");
	GENERATED_TEST_ASSERT(generated_expect_accessor_string(&out, expected, 1,
			SUITE, CASE_NAME, "lowered at(index)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected lowered at(index)");
	return generated_test_pass(SUITE, CASE_NAME);
}

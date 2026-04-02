#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/getSubstitution-0x0024N"

int
main(void)
{
	uint16_t storage[128];
	jsmethod_error_t error;
	jsstr16_t out;
	jsmethod_value_t subject = jsmethod_value_string_utf8(
			(const uint8_t *)"ABC AAA ABC AAA", 15);

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/getSubstitution-0x0024N.js
	 *
	 * The upstream custom-RegExp sample is intentionally omitted because it
	 * depends on overriding Symbol.replace above the flattened boundary.
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage), subject,
			jsmethod_value_string_utf8((const uint8_t *)"ABC", 3),
			jsmethod_value_string_utf8((const uint8_t *)"$1", 2),
			&error) == 0, SUITE, CASE_NAME,
			"failed \"$1\" literal replacement");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"$1 AAA $1 AAA", SUITE, CASE_NAME, "\"ABC\" -> \"$1\"") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected \"$1\" literal replacement");

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage), subject,
			jsmethod_value_string_utf8((const uint8_t *)"ABC", 3),
			jsmethod_value_string_utf8((const uint8_t *)"$9", 2),
			&error) == 0, SUITE, CASE_NAME,
			"failed \"$9\" literal replacement");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"$9 AAA $9 AAA", SUITE, CASE_NAME, "\"ABC\" -> \"$9\"") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected \"$9\" literal replacement");
	return generated_test_pass(SUITE, CASE_NAME);
}

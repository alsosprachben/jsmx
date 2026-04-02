#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/this-is-null-throws"

int
main(void)
{
	uint16_t storage[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/this-is-null-throws.js
	 *
	 * The upstream typeof metadata assertion is intentionally omitted.
	 */

	GENERATED_TEST_ASSERT(jsmethod_string_replace_all(&out,
			jsmethod_value_null(),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			&error) == -1, SUITE, CASE_NAME,
			"expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for null receiver, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

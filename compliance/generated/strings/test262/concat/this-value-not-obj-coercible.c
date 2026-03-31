#include <stdint.h>

#include "compliance/generated/string_concat_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/concat/this-value-not-obj-coercible"

int
main(void)
{
	uint16_t storage[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (char *)storage, sizeof(storage));

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/concat/this-value-not-obj-coercible.js
	 *
	 * This idiomatic flattened translation preserves the runtime
	 * RequireObjectCoercible(this) checks. The upstream typeof/name metadata
	 * assertions are intentionally omitted.
	 */

	GENERATED_TEST_ASSERT(jsmethod_string_concat(&out, jsmethod_value_undefined(),
			1, (const jsmethod_value_t[]){jsmethod_value_string_utf8(
				(const uint8_t *)"", 0)}, &error) == -1,
			SUITE, CASE_NAME, "expected undefined receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for undefined receiver, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsmethod_string_concat(&out, jsmethod_value_null(), 1,
			(const jsmethod_value_t[]){jsmethod_value_string_utf8(
				(const uint8_t *)"", 0)}, &error) == -1,
			SUITE, CASE_NAME, "expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for null receiver, got %d", (int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

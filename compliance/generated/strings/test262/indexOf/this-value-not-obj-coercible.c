#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/indexOf/this-value-not-obj-coercible"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/indexOf/this-value-not-obj-coercible.js
	 *
	 * The upstream file also checks `typeof String.prototype.indexOf`.
	 * This fixture keeps the method-semantics portion only.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_index_of(&index,
			jsmethod_value_undefined(),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, jsmethod_value_undefined(), &error) == -1,
			SUITE, CASE_NAME, "expected undefined receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for undefined receiver, got %d",
			(int)error.kind);
	GENERATED_TEST_ASSERT(jsmethod_string_index_of(&index,
			jsmethod_value_null(),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, jsmethod_value_undefined(), &error) == -1,
			SUITE, CASE_NAME, "expected null receiver to fail");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE, CASE_NAME,
			"expected TYPE for null receiver, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

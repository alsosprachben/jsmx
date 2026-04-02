#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/searchValue-empty-string-this-empty-string"

int
main(void)
{
	uint16_t storage[32];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/searchValue-empty-string-this-empty-string.js
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3),
			&error) == 0, SUITE, CASE_NAME,
			"failed empty-string replaceAll on empty receiver");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out, "abc", SUITE,
			CASE_NAME, "\"\".replaceAll(\"\", \"abc\")") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected empty-receiver replaceAll result");
	return generated_test_pass(SUITE, CASE_NAME);
}

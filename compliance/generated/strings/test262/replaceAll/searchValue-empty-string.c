#include <stdint.h>

#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/replaceAll/searchValue-empty-string"

int
main(void)
{
	uint16_t storage[128];
	jsmethod_error_t error;
	jsstr16_t out;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/replaceAll/searchValue-empty-string.js
	 */

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"aab c  \nx", 9),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			jsmethod_value_string_utf8((const uint8_t *)"_", 1),
			&error) == 0, SUITE, CASE_NAME,
			"failed replaceAll(empty search) on multiline input");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out,
			"_a_a_b_ _c_ _ _\n_x_", SUITE, CASE_NAME,
			"\"aab c  \\nx\".replaceAll(\"\", \"_\")") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected multiline empty-search replaceAll result");

	GENERATED_TEST_ASSERT(generated_measure_and_replace_all(&out, storage,
			GENERATED_LEN(storage),
			jsmethod_value_string_utf8((const uint8_t *)"a", 1),
			jsmethod_value_string_utf8((const uint8_t *)"", 0),
			jsmethod_value_string_utf8((const uint8_t *)"_", 1),
			&error) == 0, SUITE, CASE_NAME,
			"failed replaceAll(empty search) on one-char input");
	GENERATED_TEST_ASSERT(generated_expect_utf8_string(&out, "_a_", SUITE,
			CASE_NAME, "\"a\".replaceAll(\"\", \"_\")") ==
			GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected one-char empty-search replaceAll result");
	return generated_test_pass(SUITE, CASE_NAME);
}

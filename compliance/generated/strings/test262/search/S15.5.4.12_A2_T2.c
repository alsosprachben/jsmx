#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE_NAME "strings"
#define CASE_NAME "test262/search/S15.5.4.12_A2_T2"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Upstream source:
	 * test/built-ins/String/prototype/search/S15.5.4.12_A2_T2.js
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_search_regex(&index,
			jsmethod_value_string_utf8((const uint8_t *)"test string", 11),
			jsmethod_value_string_utf8((const uint8_t *)"String", 6),
			0, jsmethod_value_undefined(), &error) == 0,
			SUITE_NAME, CASE_NAME,
			"expected regex search to succeed");

	return generated_expect_search_index(index, -1, SUITE_NAME, CASE_NAME,
			"#1");
}

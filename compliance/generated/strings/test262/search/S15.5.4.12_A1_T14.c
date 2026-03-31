#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE_NAME "strings"
#define CASE_NAME "test262/search/S15.5.4.12_A1_T14"

int
main(void)
{
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Upstream source:
	 * test/built-ins/String/prototype/search/S15.5.4.12_A1_T14.js
	 *
	 * Idiomatic flattened translation: the RegExp object construction lowers to
	 * the explicit pattern/flags boundary used by the optional regex module.
	 */
	GENERATED_TEST_ASSERT(jsmethod_string_search_regex(&index,
			jsmethod_value_string_utf8((const uint8_t *)"ABBABABAB77BBAA", 15),
			jsmethod_value_string_utf8((const uint8_t *)"77", 2),
			0, jsmethod_value_undefined(), &error) == 0,
			SUITE_NAME, CASE_NAME,
			"expected regex-object search to succeed");

	return generated_expect_search_index(index, 9, SUITE_NAME, CASE_NAME,
			"#1");
}

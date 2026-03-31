#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE_NAME "strings"
#define CASE_NAME "test262/search/S15.5.4.12_A1_T10"

int
main(void)
{
	static const uint16_t pattern_ab[] = {'A','B'};
	uint16_t pattern_storage[sizeof(pattern_ab) / sizeof(pattern_ab[0])];
	generated_string_callback_ctx_t pattern_ctx = {
		.should_throw = 0,
		.text = pattern_ab,
		.len = sizeof(pattern_ab) / sizeof(pattern_ab[0]),
		.calls_ptr = NULL,
	};
	jsstr16_t pattern_str;
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Upstream source:
	 * test/built-ins/String/prototype/search/S15.5.4.12_A1_T10.js
	 *
	 * Idiomatic slow-path translation: the object argument's toString hook is
	 * modeled as an explicit coercion callback before the regex backend runs.
	 */
	jsstr16_init_from_buf(&pattern_str, (const char *)pattern_storage,
			sizeof(pattern_storage));
	GENERATED_TEST_ASSERT(generated_string_callback_to_string(&pattern_ctx,
			&pattern_str, &error) == 0,
			SUITE_NAME, CASE_NAME,
			"expected object-argument coercion to succeed");
	GENERATED_TEST_ASSERT(jsmethod_string_search_regex(&index,
			jsmethod_value_string_utf8((const uint8_t *)"ssABBABABAB", 11),
			jsmethod_value_string_utf16(pattern_str.codeunits, pattern_str.len),
			0, jsmethod_value_undefined(), &error) == 0,
			SUITE_NAME, CASE_NAME,
			"expected object-argument search to succeed");

	return generated_expect_search_index(index, 2, SUITE_NAME, CASE_NAME,
			"#1");
}

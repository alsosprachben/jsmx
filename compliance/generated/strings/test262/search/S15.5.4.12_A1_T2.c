#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

#define SUITE_NAME "strings"
#define CASE_NAME "test262/search/S15.5.4.12_A1_T2"

int
main(void)
{
	static const uint16_t receiver_false[] = {'f','a','l','s','e'};
	uint16_t receiver_storage[sizeof(receiver_false) / sizeof(receiver_false[0])];
	generated_string_callback_ctx_t receiver_ctx = {
		.should_throw = 0,
		.text = receiver_false,
		.len = sizeof(receiver_false) / sizeof(receiver_false[0]),
		.calls_ptr = NULL,
	};
	jsstr16_t receiver_str;
	jsmethod_error_t error;
	ssize_t index;

	/*
	 * Upstream source:
	 * test/built-ins/String/prototype/search/S15.5.4.12_A1_T2.js
	 *
	 * Idiomatic slow-path translation: the boxed Boolean receiver is lowered as
	 * an explicit receiver coercion callback outside the flattened layer.
	 */
	jsstr16_init_from_buf(&receiver_str, (const char *)receiver_storage,
			sizeof(receiver_storage));
	GENERATED_TEST_ASSERT(generated_string_callback_to_string(&receiver_ctx,
			&receiver_str, &error) == 0,
			SUITE_NAME, CASE_NAME,
			"expected boxed receiver coercion to succeed");
	GENERATED_TEST_ASSERT(jsmethod_string_search_regex(&index,
			jsmethod_value_string_utf16(receiver_str.codeunits, receiver_str.len),
			jsmethod_value_bool(0), 0, jsmethod_value_undefined(), &error) == 0,
			SUITE_NAME, CASE_NAME,
			"expected boxed receiver search to succeed");

	return generated_expect_search_index(index, 0, SUITE_NAME, CASE_NAME,
			"#1");
}

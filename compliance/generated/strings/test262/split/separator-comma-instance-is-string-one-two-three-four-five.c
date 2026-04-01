#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/separator-comma-instance-is-string-one-two-three-four-five"

int
main(void)
{
	static const uint16_t receiver_text[] = {
		'o','n','e',',','t','w','o',',','t','h','r','e','e',',',
		'f','o','u','r',',','f','i','v','e'
	};
	uint16_t receiver_storage[GENERATED_LEN(receiver_text)];
	static const char *expected[] = {"one", "two", "three", "four", "five"};
	generated_string_callback_ctx_t receiver_ctx = {
		.should_throw = 0,
		.text = receiver_text,
		.len = GENERATED_LEN(receiver_text),
		.calls_ptr = NULL,
	};
	generated_split_collect_ctx_t collect = {0};
	jsmethod_error_t error;
	jsstr16_t receiver_str;
	size_t count = 0;
	size_t i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/split/separator-comma-instance-is-string-one-two-three-four-five.js
	 *
	 * Idiomatic slow-path translation: the boxed String receiver is modeled as an
	 * explicit coercion callback outside the flattened layer.
	 */

	jsstr16_init_from_buf(&receiver_str, (char *)receiver_storage,
			sizeof(receiver_storage));
	GENERATED_TEST_ASSERT(generated_string_callback_to_string(&receiver_ctx,
			&receiver_str, &error) == 0,
			SUITE, CASE_NAME, "receiver coercion callback failed");
	GENERATED_TEST_ASSERT(jsmethod_string_split(
			jsmethod_value_string_utf16(receiver_str.codeunits, receiver_str.len),
			1, jsmethod_value_string_utf8((const uint8_t *)",", 1),
			0, jsmethod_value_undefined(),
			&collect, generated_split_collect_emit, &count, &error) == 0,
			SUITE, CASE_NAME, "split receiver callback failed");
	GENERATED_TEST_ASSERT(count == GENERATED_LEN(expected)
			&& collect.count == GENERATED_LEN(expected),
			SUITE, CASE_NAME, "expected %zu split parts, got %zu/%zu",
			(size_t)GENERATED_LEN(expected), count, collect.count);
	for (i = 0; i < GENERATED_LEN(expected); i++) {
		GENERATED_TEST_ASSERT(generated_expect_split_segment_utf8(&collect, i,
				expected[i], SUITE, CASE_NAME, "#1") ==
				GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "unexpected split segment at %zu", i);
	}
	return generated_test_pass(SUITE, CASE_NAME);
}

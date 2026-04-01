#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/split/separator-empty-string-instance-is-string"

int
main(void)
{
	static const uint16_t receiver_space[] = {' '};
	uint16_t receiver_storage[GENERATED_LEN(receiver_space)];
	generated_string_callback_ctx_t receiver_ctx = {
		.should_throw = 0,
		.text = receiver_space,
		.len = GENERATED_LEN(receiver_space),
		.calls_ptr = NULL,
	};
	generated_split_collect_ctx_t collect = {0};
	jsmethod_error_t error;
	jsstr16_t receiver_str;
	size_t count = 0;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/split/separator-empty-string-instance-is-string.js
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
			1, jsmethod_value_string_utf8((const uint8_t *)"", 0),
			0, jsmethod_value_undefined(),
			&collect, generated_split_collect_emit, &count, &error) == 0,
			SUITE, CASE_NAME, "split receiver callback failed");
	GENERATED_TEST_ASSERT(count == 1 && collect.count == 1, SUITE, CASE_NAME,
			"expected one result segment, got count=%zu collect=%zu",
			count, collect.count);
	GENERATED_TEST_ASSERT(generated_expect_split_segment_utf8(&collect, 0, " ",
			SUITE, CASE_NAME, "#1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected split segment");
	return generated_test_pass(SUITE, CASE_NAME);
}

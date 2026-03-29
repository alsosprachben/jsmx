#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/S15.5.4.16_A2_T1"

static int
expect_utf16(const char *label, jsstr16_t *actual, const uint16_t *expected,
		size_t expected_len)
{
	size_t i;

	if (actual->len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu code units, got %zu",
				label, expected_len, actual->len);
	}
	for (i = 0; i < expected_len; i++) {
		if (actual->codeunits[i] != expected[i]) {
			return generated_test_fail(SUITE, CASE_NAME,
					"%s: lowercased code units did not match expected result",
					label);
		}
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t input[] = "Hello, WoRlD!";
	static const uint16_t expected[] = {
		'h', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!',
	};
	uint16_t storage[64];
	jsstr16_t value;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toLowerCase/S15.5.4.16_A2_T1.js
	 *
	 * The original JS file also distinguishes primitive string return values
	 * from String objects. At the `jsmethod` layer, the preserved semantics are
	 * receiver coercion plus the lowercased string result itself.
	 */

	jsstr16_init_from_buf(&value, (const char *)storage, sizeof(storage));
	if (jsmethod_string_to_lower_case(&value,
			jsmethod_value_string_utf8(input, sizeof(input) - 1),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to lowercase ASCII receiver through jsmethod");
	}

	return expect_utf16("basic ASCII lowercase", &value,
			expected, sizeof(expected) / sizeof(expected[0]));
}

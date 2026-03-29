#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/S15.5.4.19_A2_T1"

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
					"%s: locale uppercased code units did not match expected result",
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
		'H', 'E', 'L', 'L', 'O', ',', ' ', 'W', 'O', 'R', 'L', 'D', '!',
	};
	uint16_t storage[64];
	jsstr16_t value;
	jsmethod_error_t error;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toLocaleUpperCase/S15.5.4.19_A2_T1.js
	 *
	 * The original JS file also distinguishes primitive string return values
	 * from String objects. At the `jsmethod` layer, the preserved semantics are
	 * receiver coercion plus the locale-method string result itself. This case
	 * uses no locale tag, so it exercises the locale API through the default
	 * locale-insensitive path.
	 */

	jsstr16_init_from_buf(&value, (const char *)storage, sizeof(storage));
	if (jsmethod_string_to_locale_upper_case(&value,
			jsmethod_value_string_utf8(input, sizeof(input) - 1),
			0, jsmethod_value_undefined(), NULL, 0, &error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to uppercase ASCII receiver through locale jsmethod");
	}

	return expect_utf16("basic locale uppercase", &value,
			expected, sizeof(expected) / sizeof(expected[0]));
}

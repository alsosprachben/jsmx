#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsstr.h"

#define SUITE "strings"
#define CASE_NAME "test262/S15.5.4.18_A2_T1"

static int
expect_utf8(const char *label, jsstr8_t *actual, const uint8_t *expected,
		size_t expected_len)
{
	if (actual->len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual->len);
	}
	if (memcmp(actual->bytes, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: uppercased bytes did not match expected result",
				label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t input[] = "Hello, WoRlD!";
	static const uint8_t expected[] = "HELLO, WORLD!";
	uint8_t storage[64];
	jsstr8_t value;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toUpperCase/S15.5.4.18_A2_T1.js
	 *
	 * The original JS file also distinguishes primitive string return values
	 * from String objects. At the `jsstr8` layer, the relevant preserved
	 * semantics are the uppercased string bytes themselves.
	 */

	jsstr8_init_from_buf(&value, (const char *)storage, sizeof(storage));
	if (jsstr8_set_from_utf8(&value, input, sizeof(input) - 1) != sizeof(input) - 1) {
		return generated_test_fail(SUITE, CASE_NAME,
				"failed to initialize UTF-8 source bytes");
	}

	jsstr8_toupper(&value);
	return expect_utf8("basic ASCII uppercase", &value,
			expected, sizeof(expected) - 1);
}

#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/intl402-toLocaleUpperCase-special_casing_Azeri"

typedef struct az_case_s {
	const uint16_t *input;
	size_t input_len;
	const uint16_t *expected;
	size_t expected_len;
	const char *label;
} az_case_t;

static int
expect_u16(jsstr16_t *actual, const uint16_t *expected, size_t expected_len,
		const char *label)
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
					"%s: mismatch at code unit %zu (expected 0x%04X, got 0x%04X)",
					label, i,
					(unsigned)expected[i],
					(unsigned)actual->codeunits[i]);
		}
	}

	return GENERATED_TEST_PASS;
}

static int
run_case(const az_case_t *tc)
{
	uint16_t storage[8];
	uint16_t locale_storage[8];
	jsstr16_t value;
	jsmethod_error_t error;

	jsstr16_init_from_buf(&value, (const char *)storage, sizeof(storage));
	if (jsmethod_string_to_locale_upper_case(&value,
			jsmethod_value_string_utf16(tc->input, tc->input_len),
			1, jsmethod_value_string_utf8((const uint8_t *)"az", 2),
			locale_storage, sizeof(locale_storage) / sizeof(locale_storage[0]),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod toLocaleUpperCase failed", tc->label);
	}

	return expect_u16(&value, tc->expected, tc->expected_len, tc->label);
}

int
main(void)
{
	static const uint16_t capital_i_with_dot[] = {0x0130};
	static const uint16_t capital_i[] = {'I'};
	static const uint16_t small_i[] = {'i'};
	static const uint16_t dotless_i[] = {0x0131};
	static const az_case_t cases[] = {
		{
			capital_i_with_dot,
			sizeof(capital_i_with_dot) / sizeof(capital_i_with_dot[0]),
			capital_i_with_dot,
			sizeof(capital_i_with_dot) / sizeof(capital_i_with_dot[0]),
			"LATIN CAPITAL LETTER I WITH DOT ABOVE",
		},
		{
			capital_i,
			sizeof(capital_i) / sizeof(capital_i[0]),
			capital_i,
			sizeof(capital_i) / sizeof(capital_i[0]),
			"LATIN CAPITAL LETTER I",
		},
		{
			small_i,
			sizeof(small_i) / sizeof(small_i[0]),
			capital_i_with_dot,
			sizeof(capital_i_with_dot) / sizeof(capital_i_with_dot[0]),
			"LATIN SMALL LETTER I",
		},
		{
			dotless_i,
			sizeof(dotless_i) / sizeof(dotless_i[0]),
			capital_i,
			sizeof(capital_i) / sizeof(capital_i[0]),
			"LATIN SMALL LETTER DOTLESS I",
		},
	};
	size_t i;

	/*
	 * Generated from test262:
	 * test/intl402/String/prototype/toLocaleUpperCase/special_casing_Azeri.js
	 *
	 * This fixture exercises the locale-conditional Azeri uppercasing path
	 * through `jsmethod_string_to_locale_upper_case(..., "az")`.
	 */

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int rc = run_case(&cases[i]);
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

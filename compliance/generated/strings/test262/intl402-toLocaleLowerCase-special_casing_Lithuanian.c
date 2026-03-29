#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/intl402-toLocaleLowerCase-special_casing_Lithuanian"

typedef struct lower_case_s {
	const char *input;
	const char *expected;
	const char *label;
} lower_case_t;

static int
hex_digit_value(char c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return 10 + (c - 'a');
	}
	if (c >= 'A' && c <= 'F') {
		return 10 + (c - 'A');
	}
	return -1;
}

static int
load_escaped_u16(jsstr16_t *value, uint16_t *storage, size_t storage_cap,
		const char *escaped, const char *label)
{
	const char *p;

	jsstr16_init_from_buf(value, (const char *)storage,
			storage_cap * sizeof(storage[0]));
	for (p = escaped; *p; ) {
		if (*p == '\\') {
			int d0, d1, d2, d3;
			uint16_t cu;

			if (p[1] != 'u') {
				return generated_test_fail(SUITE, CASE_NAME,
						"%s: unsupported escape sequence", label);
			}
			d0 = hex_digit_value(p[2]);
			d1 = hex_digit_value(p[3]);
			d2 = hex_digit_value(p[4]);
			d3 = hex_digit_value(p[5]);
			if (d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0) {
				return generated_test_fail(SUITE, CASE_NAME,
						"%s: invalid hex escape", label);
			}
			if (value->len >= value->cap) {
				return generated_test_fail(SUITE, CASE_NAME,
						"%s: escaped string exceeded fixture capacity", label);
			}
			cu = (uint16_t)((d0 << 12) | (d1 << 8) | (d2 << 4) | d3);
			value->codeunits[value->len++] = cu;
			p += 6;
			continue;
		}

		if (value->len >= value->cap) {
			return generated_test_fail(SUITE, CASE_NAME,
					"%s: raw string exceeded fixture capacity", label);
		}
		value->codeunits[value->len++] = (uint16_t)(unsigned char)*p++;
	}

	return GENERATED_TEST_PASS;
}

static int
expect_equal_u16(jsstr16_t *actual, jsstr16_t *expected, const char *label)
{
	size_t i;

	if (actual->len != expected->len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu code units, got %zu",
				label, expected->len, actual->len);
	}
	for (i = 0; i < expected->len; i++) {
		if (actual->codeunits[i] != expected->codeunits[i]) {
			return generated_test_fail(SUITE, CASE_NAME,
					"%s: mismatch at code unit %zu (expected 0x%04X, got 0x%04X)",
					label, i,
					(unsigned)expected->codeunits[i],
					(unsigned)actual->codeunits[i]);
		}
	}

	return GENERATED_TEST_PASS;
}

static int
run_case(const lower_case_t *tc)
{
	uint16_t input_storage[24];
	uint16_t expected_storage[24];
	uint16_t locale_storage[8];
	jsstr16_t input;
	jsstr16_t expected;
	jsmethod_error_t error;
	int rc;

	rc = load_escaped_u16(&input, input_storage,
			sizeof(input_storage) / sizeof(input_storage[0]),
			tc->input, tc->label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}
	rc = load_escaped_u16(&expected, expected_storage,
			sizeof(expected_storage) / sizeof(expected_storage[0]),
			tc->expected, tc->label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	if (jsmethod_string_to_locale_lower_case(&input,
			jsmethod_value_string_utf16(input.codeunits, input.len),
			1, jsmethod_value_string_utf8((const uint8_t *)"lt", 2),
			locale_storage, sizeof(locale_storage) / sizeof(locale_storage[0]),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod toLocaleLowerCase failed", tc->label);
	}
	return expect_equal_u16(&input, &expected, tc->label);
}

int
main(void)
{
	static const lower_case_t cases[] = {
		{"I\\u0300", "i\\u0307\\u0300", "LATIN CAPITAL LETTER I followed by COMBINING GRAVE ACCENT"},
		{"J\\u0300", "j\\u0307\\u0300", "LATIN CAPITAL LETTER J followed by COMBINING GRAVE ACCENT"},
		{"\\u012E\\u0300", "\\u012F\\u0307\\u0300", "LATIN CAPITAL LETTER I WITH OGONEK followed by COMBINING GRAVE ACCENT"},
		{"I\\uD834\\uDD85", "i\\u0307\\uD834\\uDD85", "LATIN CAPITAL LETTER I followed by MUSICAL SYMBOL COMBINING DOIT"},
		{"J\\uD834\\uDD85", "j\\u0307\\uD834\\uDD85", "LATIN CAPITAL LETTER J followed by MUSICAL SYMBOL COMBINING DOIT"},
		{"\\u012E\\uD834\\uDD85", "\\u012F\\u0307\\uD834\\uDD85", "LATIN CAPITAL LETTER I WITH OGONEK followed by MUSICAL SYMBOL COMBINING DOIT"},
		{"I\\u0325\\u0300", "i\\u0307\\u0325\\u0300", "LATIN CAPITAL LETTER I followed by COMBINING RING BELOW, COMBINING GRAVE ACCENT"},
		{"J\\u0325\\u0300", "j\\u0307\\u0325\\u0300", "LATIN CAPITAL LETTER J followed by COMBINING RING BELOW, COMBINING GRAVE ACCENT"},
		{"\\u012E\\u0325\\u0300", "\\u012F\\u0307\\u0325\\u0300", "LATIN CAPITAL LETTER I WITH OGONEK followed by COMBINING RING BELOW, COMBINING GRAVE ACCENT"},
		{"I\\uD800\\uDDFD\\u0300", "i\\u0307\\uD800\\uDDFD\\u0300", "LATIN CAPITAL LETTER I followed by PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, COMBINING GRAVE ACCENT"},
		{"J\\uD800\\uDDFD\\u0300", "j\\u0307\\uD800\\uDDFD\\u0300", "LATIN CAPITAL LETTER J followed by PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, COMBINING GRAVE ACCENT"},
		{"\\u012E\\uD800\\uDDFD\\u0300", "\\u012F\\u0307\\uD800\\uDDFD\\u0300", "LATIN CAPITAL LETTER I WITH OGONEK followed by PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, COMBINING GRAVE ACCENT"},
		{"I\\u0325\\uD834\\uDD85", "i\\u0307\\u0325\\uD834\\uDD85", "LATIN CAPITAL LETTER I followed by COMBINING RING BELOW, MUSICAL SYMBOL COMBINING DOIT"},
		{"J\\u0325\\uD834\\uDD85", "j\\u0307\\u0325\\uD834\\uDD85", "LATIN CAPITAL LETTER J followed by COMBINING RING BELOW, MUSICAL SYMBOL COMBINING DOIT"},
		{"\\u012E\\u0325\\uD834\\uDD85", "\\u012F\\u0307\\u0325\\uD834\\uDD85", "LATIN CAPITAL LETTER I WITH OGONEK followed by COMBINING RING BELOW, MUSICAL SYMBOL COMBINING DOIT"},
		{"I\\uD800\\uDDFD\\uD834\\uDD85", "i\\u0307\\uD800\\uDDFD\\uD834\\uDD85", "LATIN CAPITAL LETTER I followed by PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, MUSICAL SYMBOL COMBINING DOIT"},
		{"J\\uD800\\uDDFD\\uD834\\uDD85", "j\\u0307\\uD800\\uDDFD\\uD834\\uDD85", "LATIN CAPITAL LETTER J followed by PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, MUSICAL SYMBOL COMBINING DOIT"},
		{"\\u012E\\uD800\\uDDFD\\uD834\\uDD85", "\\u012F\\u0307\\uD800\\uDDFD\\uD834\\uDD85", "LATIN CAPITAL LETTER I WITH OGONEK followed by PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, MUSICAL SYMBOL COMBINING DOIT"},
		{"IA\\u0300", "ia\\u0300", "LATIN CAPITAL LETTER I followed by LATIN CAPITAL LETTER A, COMBINING GRAVE ACCENT"},
		{"JA\\u0300", "ja\\u0300", "LATIN CAPITAL LETTER J followed by LATIN CAPITAL LETTER A, COMBINING GRAVE ACCENT"},
		{"\\u012EA\\u0300", "\\u012Fa\\u0300", "LATIN CAPITAL LETTER I WITH OGONEK followed by LATIN CAPITAL LETTER A, COMBINING GRAVE ACCENT"},
		{"IA\\uD834\\uDD85", "ia\\uD834\\uDD85", "LATIN CAPITAL LETTER I followed by LATIN CAPITAL LETTER A, MUSICAL SYMBOL COMBINING DOIT"},
		{"JA\\uD834\\uDD85", "ja\\uD834\\uDD85", "LATIN CAPITAL LETTER J followed by LATIN CAPITAL LETTER A, MUSICAL SYMBOL COMBINING DOIT"},
		{"\\u012EA\\uD834\\uDD85", "\\u012Fa\\uD834\\uDD85", "LATIN CAPITAL LETTER I WITH OGONEK followed by LATIN CAPITAL LETTER A, MUSICAL SYMBOL COMBINING DOIT"},
		{"\\u00CC", "\\u0069\\u0307\\u0300", "LATIN CAPITAL LETTER I WITH GRAVE"},
		{"\\u00CD", "\\u0069\\u0307\\u0301", "LATIN CAPITAL LETTER I WITH ACUTE"},
		{"\\u0128", "\\u0069\\u0307\\u0303", "LATIN CAPITAL LETTER I WITH TILDE"},
	};
	size_t i;

	/*
	 * Generated from test262:
	 * test/intl402/String/prototype/toLocaleLowerCase/special_casing_Lithuanian.js
	 *
	 * This fixture exercises the Lithuanian locale-conditional lowercase path
	 * through `jsmethod_string_to_locale_lower_case(..., "lt")`.
	 */

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int rc = run_case(&cases[i]);
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

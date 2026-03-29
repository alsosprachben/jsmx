#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsstr.h"
#include "unicode.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-normalized-string-from-coerced-form"

typedef struct normalize_case_s {
	const char *form;
	size_t form_len;
	unicode_normalization_form_t expected_form;
	const uint16_t *expected;
	size_t expected_len;
	const char *label;
} normalize_case_t;

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
					"%s: mismatch at code unit %zu (expected 0x%04X, got 0x%04X)",
					label, i,
					(unsigned)expected[i],
					(unsigned)actual->codeunits[i]);
		}
	}

	return GENERATED_TEST_PASS;
}

static int
run_case(const normalize_case_t *tc)
{
	static const uint16_t input[] = {
		0x00C5, 0x2ADC, 0x0958, 0x2126, 0x0344,
	};
	uint16_t storage[24];
	jsstr16_t value;
	unicode_normalization_form_t form;
	size_t written;

	errno = 0;
	if (unicode_normalization_form_parse(tc->form, tc->form_len, &form) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to parse coerced form", tc->label);
	}
	if (form != tc->expected_form) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: parsed to wrong normalization form", tc->label);
	}

	jsstr16_init_from_buf(&value, (const char *)storage, sizeof(storage));
	written = jsstr16_set_from_utf16(&value, input,
			sizeof(input) / sizeof(input[0]));
	if (written != sizeof(input) / sizeof(input[0])) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected to write source string", tc->label);
	}

	jsstr16_normalize_form(&value, form);
	return expect_utf16(tc->label, &value, tc->expected, tc->expected_len);
}

int
main(void)
{
	static const uint16_t expected_nfc[] = {
		0x00C5, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	static const uint16_t expected_nfd[] = {
		0x0041, 0x030A, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	static const normalize_case_t cases[] = {
		{"NFC", 3, UNICODE_NORMALIZE_NFC, expected_nfc,
		 sizeof(expected_nfc) / sizeof(expected_nfc[0]), "coerced array - NFC"},
		{"NFC", 3, UNICODE_NORMALIZE_NFC, expected_nfc,
		 sizeof(expected_nfc) / sizeof(expected_nfc[0]), "coerced object - NFC"},
		{"NFD", 3, UNICODE_NORMALIZE_NFD, expected_nfd,
		 sizeof(expected_nfd) / sizeof(expected_nfd[0]), "coerced array - NFD"},
		{"NFD", 3, UNICODE_NORMALIZE_NFD, expected_nfd,
		 sizeof(expected_nfd) / sizeof(expected_nfd[0]), "coerced object - NFD"},
	};
	size_t i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/normalize/return-normalized-string-from-coerced-form.js
	 *
	 * The original JS file proves that array/object inputs coerce to `"NFC"` and
	 * `"NFD"`. At the `jsmx` layer, that coercion has already collapsed to a
	 * form string, which we parse through `unicode_normalization_form_parse`.
	 */

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int rc = run_case(&cases[i]);
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

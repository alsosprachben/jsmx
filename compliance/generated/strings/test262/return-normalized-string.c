#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsstr.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-normalized-string"

typedef struct normalize_case_s {
	const uint16_t *input;
	size_t input_len;
	unicode_normalization_form_t form;
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
	uint16_t storage[24];
	jsstr16_t value;
	size_t written;

	jsstr16_init_from_buf(&value, (const char *)storage, sizeof(storage));
	written = jsstr16_set_from_utf16(&value, tc->input, tc->input_len);
	if (written != tc->input_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected to write %zu code units, got %zu",
				tc->label, tc->input_len, written);
	}

	jsstr16_normalize_form(&value, tc->form);
	return expect_utf16(tc->label, &value, tc->expected, tc->expected_len);
}

int
main(void)
{
	static const uint16_t input_a[] = {0x1E9B, 0x0323};
	static const uint16_t input_b[] = {0x00C5, 0x2ADC, 0x0958, 0x2126, 0x0344};
	static const uint16_t expected_a_nfc[] = {0x1E9B, 0x0323};
	static const uint16_t expected_a_nfd[] = {0x017F, 0x0323, 0x0307};
	static const uint16_t expected_a_nfkc[] = {0x1E69};
	static const uint16_t expected_a_nfkd[] = {0x0073, 0x0323, 0x0307};
	static const uint16_t expected_b_nfc[] = {
		0x00C5, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	static const uint16_t expected_b_nfd[] = {
		0x0041, 0x030A, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	static const uint16_t expected_b_nfkc[] = {
		0x00C5, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	static const uint16_t expected_b_nfkd[] = {
		0x0041, 0x030A, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	static const normalize_case_t cases[] = {
		{input_a, 2, UNICODE_NORMALIZE_NFC, expected_a_nfc, 2, "input A normalized on NFC"},
		{input_a, 2, UNICODE_NORMALIZE_NFD, expected_a_nfd, 3, "input A normalized on NFD"},
		{input_a, 2, UNICODE_NORMALIZE_NFKC, expected_a_nfkc, 1, "input A normalized on NFKC"},
		{input_a, 2, UNICODE_NORMALIZE_NFKD, expected_a_nfkd, 3, "input A normalized on NFKD"},
		{input_b, 5, UNICODE_NORMALIZE_NFC, expected_b_nfc, 8, "input B normalized on NFC"},
		{input_b, 5, UNICODE_NORMALIZE_NFD, expected_b_nfd, 9, "input B normalized on NFD"},
		{input_b, 5, UNICODE_NORMALIZE_NFKC, expected_b_nfkc, 8, "input B normalized on NFKC"},
		{input_b, 5, UNICODE_NORMALIZE_NFKD, expected_b_nfkd, 9, "input B normalized on NFKD"},
	};
	size_t i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/normalize/return-normalized-string.js
	 *
	 * The JS normalization-form strings are translated directly to the explicit
	 * `unicode_normalization_form_t` API exposed through `jsstr16_normalize_form`.
	 */

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		int rc = run_case(&cases[i]);
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

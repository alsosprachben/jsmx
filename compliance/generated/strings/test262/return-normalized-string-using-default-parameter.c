#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsstr.h"

#define SUITE "strings"
#define CASE_NAME "test262/return-normalized-string-using-default-parameter"

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
set_utf16(jsstr16_t *value, uint16_t *storage, size_t storage_len,
		const uint16_t *input, size_t input_len, const char *label)
{
	size_t written;

	jsstr16_init_from_buf(value, (const char *)storage,
			storage_len * sizeof(storage[0]));
	written = jsstr16_set_from_utf16(value, input, input_len);
	if (written != input_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected to write %zu code units, got %zu",
				label, input_len, written);
	}

	return GENERATED_TEST_PASS;
}

static int
run_default_nfc_case(const char *label)
{
	static const uint16_t input[] = {
		0x00C5, 0x2ADC, 0x0958, 0x2126, 0x0344,
	};
	static const uint16_t expected[] = {
		0x00C5, 0x2ADD, 0x0338, 0x0915, 0x093C, 0x03A9, 0x0308, 0x0301,
	};
	uint16_t storage[16];
	jsstr16_t value;
	int rc;

	rc = set_utf16(&value, storage, sizeof(storage) / sizeof(storage[0]),
			input, sizeof(input) / sizeof(input[0]), label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	jsstr16_normalize(&value);
	return expect_utf16(label, &value, expected,
			sizeof(expected) / sizeof(expected[0]));
}

int
main(void)
{
	int rc;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/normalize/return-normalized-string-using-default-parameter.js
	 *
	 * `jsmx` currently exposes NFC normalization through `jsstr16_normalize()`.
	 * The distinct JS call shapes `normalize()` and `normalize(undefined)`
	 * therefore collapse to the same library-level operation here.
	 */

	rc = run_default_nfc_case("Use NFC as the default form");
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_default_nfc_case("Use NFC as the default form when argument is undefined");
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/returns-well-formed-string"

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
run_to_well_formed_case(const char *label, const uint16_t *input,
		size_t input_len, const uint16_t *expected, size_t expected_len)
{
	uint16_t storage[16];
	jsstr16_t value;
	jsmethod_error_t error;
	int rc;

	rc = set_utf16(&value, storage, sizeof(storage) / sizeof(storage[0]),
			input, input_len, label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	if (jsmethod_string_to_well_formed(&value,
			jsmethod_value_string_utf16(input, input_len),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod toWellFormed failed", label);
	}
	return expect_utf16(label, &value, expected, expected_len);
}

static int
run_sliced_surrogate_case(const char *label, size_t start_i, ssize_t stop_i,
		const uint16_t *expected, size_t expected_len)
{
	static const uint16_t whole_poo[] = {0xD83D, 0xDCA9};
	uint16_t storage[4];
	jsstr16_t whole;
	jsstr16_t slice;
	jsmethod_error_t error;
	int rc;

	rc = set_utf16(&whole, storage, sizeof(storage) / sizeof(storage[0]),
			whole_poo, sizeof(whole_poo) / sizeof(whole_poo[0]), label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	jsstr16_u16_slice(&slice, &whole, start_i, stop_i);
	if (jsmethod_string_to_well_formed(&slice,
			jsmethod_value_string_utf16(slice.codeunits, slice.len),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod toWellFormed failed for slice", label);
	}
	return expect_utf16(label, &slice, expected, expected_len);
}

int
main(void)
{
	static const uint16_t replacement_char[] = {0xFFFD};
	static const uint16_t lone_leading_input[] = {'a', 0xD83D, 'c', 0xD83D, 'e'};
	static const uint16_t lone_leading_expected[] = {'a', 0xFFFD, 'c', 0xFFFD, 'e'};
	static const uint16_t lone_trailing_input[] = {'a', 0xDCA9, 'c', 0xDCA9, 'e'};
	static const uint16_t lone_trailing_expected[] = {'a', 0xFFFD, 'c', 0xFFFD, 'e'};
	static const uint16_t wrong_order_input[] = {'a', 0xDCA9, 0xD83D, 'd'};
	static const uint16_t wrong_order_expected[] = {'a', 0xFFFD, 0xFFFD, 'd'};
	static const uint16_t well_formed_pair[] = {'a', 0xD83D, 0xDCA9, 'c'};
	static const uint16_t concatenated_pair[] = {'a', 0xD83D, 0xDCA9, 'd'};
	static const uint16_t ascii_input[] = {'a', 'b', 'c'};
	static const uint16_t non_ascii_input[] = {'a', 0x25A8, 'c'};
	int rc;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toWellFormed/returns-well-formed-string.js
	 *
	 * The original JS file also checks `typeof String.prototype.toWellFormed`.
	 * That prototype-shape assertion is outside the thin method layer here, so
	 * this fixture keeps only the receiver-coercion and string-behavior cases.
	 */

	rc = run_to_well_formed_case(
			"lone leading surrogates are replaced",
			lone_leading_input,
			sizeof(lone_leading_input) / sizeof(lone_leading_input[0]),
			lone_leading_expected,
			sizeof(lone_leading_expected) / sizeof(lone_leading_expected[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"lone trailing surrogates are replaced",
			lone_trailing_input,
			sizeof(lone_trailing_input) / sizeof(lone_trailing_input[0]),
			lone_trailing_expected,
			sizeof(lone_trailing_expected) / sizeof(lone_trailing_expected[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"wrong-ordered surrogate pair becomes two replacements",
			wrong_order_input,
			sizeof(wrong_order_input) / sizeof(wrong_order_input[0]),
			wrong_order_expected,
			sizeof(wrong_order_expected) / sizeof(wrong_order_expected[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"surrogate pair literal stays well formed",
			well_formed_pair,
			sizeof(well_formed_pair) / sizeof(well_formed_pair[0]),
			well_formed_pair,
			sizeof(well_formed_pair) / sizeof(well_formed_pair[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"surrogate pair escapes stay well formed",
			well_formed_pair,
			sizeof(well_formed_pair) / sizeof(well_formed_pair[0]),
			well_formed_pair,
			sizeof(well_formed_pair) / sizeof(well_formed_pair[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"concatenated surrogate pair stays well formed",
			concatenated_pair,
			sizeof(concatenated_pair) / sizeof(concatenated_pair[0]),
			concatenated_pair,
			sizeof(concatenated_pair) / sizeof(concatenated_pair[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_sliced_surrogate_case(
			"slice to leading surrogate is replaced",
			0, 1,
			replacement_char,
			sizeof(replacement_char) / sizeof(replacement_char[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_sliced_surrogate_case(
			"slice to trailing surrogate is replaced",
			1, -1,
			replacement_char,
			sizeof(replacement_char) / sizeof(replacement_char[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"latin-1 stays well formed",
			ascii_input,
			sizeof(ascii_input) / sizeof(ascii_input[0]),
			ascii_input,
			sizeof(ascii_input) / sizeof(ascii_input[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_to_well_formed_case(
			"non-ASCII BMP character stays well formed",
			non_ascii_input,
			sizeof(non_ascii_input) / sizeof(non_ascii_input[0]),
			non_ascii_input,
			sizeof(non_ascii_input) / sizeof(non_ascii_input[0]));
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/returns-boolean"

static int
run_is_well_formed_case(const char *label, const uint16_t *input,
		size_t input_len, int expected)
{
	uint16_t storage[16];
	jsmethod_error_t error;
	int is_well_formed;

	if (jsmethod_string_is_well_formed(&is_well_formed,
			jsmethod_value_string_utf16(input, input_len),
			storage, sizeof(storage) / sizeof(storage[0]),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod isWellFormed failed", label);
	}
	if (is_well_formed != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %d, got %d", label, expected, is_well_formed);
	}
	return GENERATED_TEST_PASS;
}

static int
run_sliced_surrogate_case(const char *label, size_t start_i, ssize_t stop_i,
		int expected)
{
	static const uint16_t whole_poo[] = {0xD83D, 0xDCA9};
	uint16_t storage[4];
	jsstr16_t whole;
	jsstr16_t slice;
	jsmethod_error_t error;
	int is_well_formed;

	jsstr16_init_from_buf(&whole, (const char *)storage, sizeof(storage));
	if (jsstr16_set_from_utf16(&whole, whole_poo,
			sizeof(whole_poo) / sizeof(whole_poo[0])) !=
			sizeof(whole_poo) / sizeof(whole_poo[0])) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to set surrogate pair input", label);
	}

	jsstr16_u16_slice(&slice, &whole, start_i, stop_i);
	if (jsmethod_string_is_well_formed(&is_well_formed,
			jsmethod_value_string_utf16(slice.codeunits, slice.len),
			storage, sizeof(storage) / sizeof(storage[0]),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod isWellFormed failed for slice", label);
	}
	if (is_well_formed != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %d, got %d", label, expected, is_well_formed);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint16_t lone_leading_input[] = {'a', 0xD83D, 'c', 0xD83D, 'e'};
	static const uint16_t lone_trailing_input[] = {'a', 0xDCA9, 'c', 0xDCA9, 'e'};
	static const uint16_t wrong_order_input[] = {'a', 0xDCA9, 0xD83D, 'd'};
	static const uint16_t well_formed_pair[] = {'a', 0xD83D, 0xDCA9, 'c'};
	static const uint16_t concatenated_pair[] = {'a', 0xD83D, 0xDCA9, 'd'};
	static const uint16_t ascii_input[] = {'a', 'b', 'c'};
	static const uint16_t non_ascii_input[] = {'a', 0x25A8, 'c'};
	int rc;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/isWellFormed/returns-boolean.js
	 *
	 * The original JS file also checks `typeof String.prototype.isWellFormed`.
	 * That prototype-shape assertion is outside the thin method layer here, so
	 * this fixture keeps only the receiver-coercion and boolean string-behavior
	 * cases.
	 */

	rc = run_is_well_formed_case(
			"lone leading surrogates are not well formed",
			lone_leading_input,
			sizeof(lone_leading_input) / sizeof(lone_leading_input[0]),
			0);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"lone trailing surrogates are not well formed",
			lone_trailing_input,
			sizeof(lone_trailing_input) / sizeof(lone_trailing_input[0]),
			0);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"wrong-ordered surrogate pair is not well formed",
			wrong_order_input,
			sizeof(wrong_order_input) / sizeof(wrong_order_input[0]),
			0);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"surrogate pair literal is well formed",
			well_formed_pair,
			sizeof(well_formed_pair) / sizeof(well_formed_pair[0]),
			1);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"surrogate pair escapes are well formed",
			well_formed_pair,
			sizeof(well_formed_pair) / sizeof(well_formed_pair[0]),
			1);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"concatenated surrogate pair is well formed",
			concatenated_pair,
			sizeof(concatenated_pair) / sizeof(concatenated_pair[0]),
			1);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_sliced_surrogate_case(
			"slice to leading surrogate is not well formed",
			0, 1, 0);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_sliced_surrogate_case(
			"slice to trailing surrogate is not well formed",
			1, -1, 0);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"latin-1 string is well formed",
			ascii_input,
			sizeof(ascii_input) / sizeof(ascii_input[0]),
			1);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	rc = run_is_well_formed_case(
			"non-ASCII BMP string is well formed",
			non_ascii_input,
			sizeof(non_ascii_input) / sizeof(non_ascii_input[0]),
			1);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

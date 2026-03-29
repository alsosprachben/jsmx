#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsstr.h"

#define SUITE "strings"
#define CASE_NAME "test262/intl402-toLocaleUpperCase-special_casing_Lithuanian"

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
run_literal_case(const char *label, const char *input_escaped,
		const char *expected_escaped)
{
	uint16_t input_storage[16];
	uint16_t expected_storage[16];
	jsstr16_t input;
	jsstr16_t expected;
	int rc;

	rc = load_escaped_u16(&input, input_storage,
			sizeof(input_storage) / sizeof(input_storage[0]),
			input_escaped, label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}
	rc = load_escaped_u16(&expected, expected_storage,
			sizeof(expected_storage) / sizeof(expected_storage[0]),
			expected_escaped, label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	jsstr16_toupper_locale(&input, "lt");
	return expect_equal_u16(&input, &expected, label);
}

static int
run_soft_dotted_case(const char *label, const char *base_escaped,
		const char *input_suffix, const char *expected_suffix)
{
	uint16_t base_storage[16];
	uint16_t input_storage[32];
	uint16_t expected_storage[32];
	uint16_t scratch_storage[16];
	jsstr16_t base;
	jsstr16_t input;
	jsstr16_t expected;
	jsstr16_t suffix;
	int rc;

	rc = load_escaped_u16(&base, base_storage,
			sizeof(base_storage) / sizeof(base_storage[0]),
			base_escaped, label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	jsstr16_init_from_buf(&input, (const char *)input_storage,
			sizeof(input_storage));
	jsstr16_set_from_utf16(&input, base.codeunits, base.len);

	jsstr16_init_from_buf(&suffix, (const char *)scratch_storage,
			sizeof(scratch_storage));
	rc = load_escaped_u16(&suffix, scratch_storage,
			sizeof(scratch_storage) / sizeof(scratch_storage[0]),
			input_suffix, label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}
	if (jsstr16_concat(&input, &suffix) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: could not append input suffix", label);
	}

	jsstr16_init_from_buf(&expected, (const char *)expected_storage,
			sizeof(expected_storage));
	jsstr16_set_from_utf16(&expected, base.codeunits, base.len);
	jsstr16_toupper_locale(&expected, "und");

	jsstr16_init_from_buf(&suffix, (const char *)scratch_storage,
			sizeof(scratch_storage));
	rc = load_escaped_u16(&suffix, scratch_storage,
			sizeof(scratch_storage) / sizeof(scratch_storage[0]),
			expected_suffix, label);
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}
	if (jsstr16_concat(&expected, &suffix) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: could not append expected suffix", label);
	}

	jsstr16_toupper_locale(&input, "lt");
	return expect_equal_u16(&input, &expected, label);
}

int
main(void)
{
	static const char *soft_dotted[] = {
		"\\u0069", "\\u006A",
		"\\u012F",
		"\\u0249",
		"\\u0268",
		"\\u029D",
		"\\u02B2",
		"\\u03F3",
		"\\u0456",
		"\\u0458",
		"\\u1D62",
		"\\u1D96",
		"\\u1DA4",
		"\\u1DA8",
		"\\u1E2D",
		"\\u1ECB",
		"\\u2071",
		"\\u2148", "\\u2149",
		"\\u2C7C",
		"\\uD835\\uDC22", "\\uD835\\uDC23",
		"\\uD835\\uDC56", "\\uD835\\uDC57",
		"\\uD835\\uDC8A", "\\uD835\\uDC8B",
		"\\uD835\\uDCBE", "\\uD835\\uDCBF",
		"\\uD835\\uDCF2", "\\uD835\\uDCF3",
		"\\uD835\\uDD26", "\\uD835\\uDD27",
		"\\uD835\\uDD5A", "\\uD835\\uDD5B",
		"\\uD835\\uDD8E", "\\uD835\\uDD8F",
		"\\uD835\\uDDC2", "\\uD835\\uDDC3",
		"\\uD835\\uDDF6", "\\uD835\\uDDF7",
		"\\uD835\\uDE2A", "\\uD835\\uDE2B",
		"\\uD835\\uDE5E", "\\uD835\\uDE5F",
		"\\uD835\\uDE92", "\\uD835\\uDE93",
	};
	size_t i;
	int rc;

	/*
	 * Generated from test262:
	 * test/intl402/String/prototype/toLocaleUpperCase/special_casing_Lithuanian.js
	 *
	 * This fixture keeps the upstream escaped code-unit corpus and exercises
	 * the locale-conditional Lithuanian uppercasing path through
	 * `jsstr16_toupper_locale(..., "lt")`.
	 */

	rc = run_literal_case(
			"COMBINING DOT ABOVE preceded by LATIN CAPITAL LETTER I",
			"I\\u0307",
			"I\\u0307");
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}
	rc = run_literal_case(
			"COMBINING DOT ABOVE preceded by LATIN CAPITAL LETTER J",
			"J\\u0307",
			"J\\u0307");
	if (rc != GENERATED_TEST_PASS) {
		return rc;
	}

	for (i = 0; i < sizeof(soft_dotted) / sizeof(soft_dotted[0]); i++) {
		char label[128];

		snprintf(label, sizeof(label),
				"soft-dotted direct dot-above removal #%zu", i);
		rc = run_soft_dotted_case(label, soft_dotted[i],
				"\\u0307", "");
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}

		snprintf(label, sizeof(label),
				"soft-dotted dot-below then dot-above #%zu", i);
		rc = run_soft_dotted_case(label, soft_dotted[i],
				"\\u0323\\u0307", "\\u0323");
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}

		snprintf(label, sizeof(label),
				"soft-dotted phaistos stroke then dot-above #%zu", i);
		rc = run_soft_dotted_case(label, soft_dotted[i],
				"\\uD800\\uDDFD\\u0307", "\\uD800\\uDDFD");
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

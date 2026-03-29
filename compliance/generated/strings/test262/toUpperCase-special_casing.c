#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsmethod.h"

#define SUITE "strings"
#define CASE_NAME "test262/toUpperCase-special_casing"

typedef struct special_case_s {
	const char *input;
	const char *expected;
	const char *label;
} special_case_t;

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
run_special_case(const special_case_t *tc)
{
	uint16_t input_storage[16];
	uint16_t expected_storage[16];
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

	if (jsmethod_string_to_upper_case(&input,
			jsmethod_value_string_utf16(input.codeunits, input.len),
			&error) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsmethod toUpperCase failed", tc->label);
	}
	return expect_equal_u16(&input, &expected, tc->label);
}

static const special_case_t special_cases[] = {
	{"\\u00DF", "\\u0053\\u0053", "LATIN SMALL LETTER SHARP S"},
	{"\\u0130", "\\u0130", "LATIN CAPITAL LETTER I WITH DOT ABOVE"},
	{"\\uFB00", "\\u0046\\u0046", "LATIN SMALL LIGATURE FF"},
	{"\\uFB01", "\\u0046\\u0049", "LATIN SMALL LIGATURE FI"},
	{"\\uFB02", "\\u0046\\u004C", "LATIN SMALL LIGATURE FL"},
	{"\\uFB03", "\\u0046\\u0046\\u0049", "LATIN SMALL LIGATURE FFI"},
	{"\\uFB04", "\\u0046\\u0046\\u004C", "LATIN SMALL LIGATURE FFL"},
	{"\\uFB05", "\\u0053\\u0054", "LATIN SMALL LIGATURE LONG S T"},
	{"\\uFB06", "\\u0053\\u0054", "LATIN SMALL LIGATURE ST"},
	{"\\u0587", "\\u0535\\u0552", "ARMENIAN SMALL LIGATURE ECH YIWN"},
	{"\\uFB13", "\\u0544\\u0546", "ARMENIAN SMALL LIGATURE MEN NOW"},
	{"\\uFB14", "\\u0544\\u0535", "ARMENIAN SMALL LIGATURE MEN ECH"},
	{"\\uFB15", "\\u0544\\u053B", "ARMENIAN SMALL LIGATURE MEN INI"},
	{"\\uFB16", "\\u054E\\u0546", "ARMENIAN SMALL LIGATURE VEW NOW"},
	{"\\uFB17", "\\u0544\\u053D", "ARMENIAN SMALL LIGATURE MEN XEH"},
	{"\\u0149", "\\u02BC\\u004E", "LATIN SMALL LETTER N PRECEDED BY APOSTROPHE"},
	{"\\u0390", "\\u0399\\u0308\\u0301", "GREEK SMALL LETTER IOTA WITH DIALYTIKA AND TONOS"},
	{"\\u03B0", "\\u03A5\\u0308\\u0301", "GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND TONOS"},
	{"\\u01F0", "\\u004A\\u030C", "LATIN SMALL LETTER J WITH CARON"},
	{"\\u1E96", "\\u0048\\u0331", "LATIN SMALL LETTER H WITH LINE BELOW"},
	{"\\u1E97", "\\u0054\\u0308", "LATIN SMALL LETTER T WITH DIAERESIS"},
	{"\\u1E98", "\\u0057\\u030A", "LATIN SMALL LETTER W WITH RING ABOVE"},
	{"\\u1E99", "\\u0059\\u030A", "LATIN SMALL LETTER Y WITH RING ABOVE"},
	{"\\u1E9A", "\\u0041\\u02BE", "LATIN SMALL LETTER A WITH RIGHT HALF RING"},
	{"\\u1F50", "\\u03A5\\u0313", "GREEK SMALL LETTER UPSILON WITH PSILI"},
	{"\\u1F52", "\\u03A5\\u0313\\u0300", "GREEK SMALL LETTER UPSILON WITH PSILI AND VARIA"},
	{"\\u1F54", "\\u03A5\\u0313\\u0301", "GREEK SMALL LETTER UPSILON WITH PSILI AND OXIA"},
	{"\\u1F56", "\\u03A5\\u0313\\u0342", "GREEK SMALL LETTER UPSILON WITH PSILI AND PERISPOMENI"},
	{"\\u1FB6", "\\u0391\\u0342", "GREEK SMALL LETTER ALPHA WITH PERISPOMENI"},
	{"\\u1FC6", "\\u0397\\u0342", "GREEK SMALL LETTER ETA WITH PERISPOMENI"},
	{"\\u1FD2", "\\u0399\\u0308\\u0300", "GREEK SMALL LETTER IOTA WITH DIALYTIKA AND VARIA"},
	{"\\u1FD3", "\\u0399\\u0308\\u0301", "GREEK SMALL LETTER IOTA WITH DIALYTIKA AND OXIA"},
	{"\\u1FD6", "\\u0399\\u0342", "GREEK SMALL LETTER IOTA WITH PERISPOMENI"},
	{"\\u1FD7", "\\u0399\\u0308\\u0342", "GREEK SMALL LETTER IOTA WITH DIALYTIKA AND PERISPOMENI"},
	{"\\u1FE2", "\\u03A5\\u0308\\u0300", "GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND VARIA"},
	{"\\u1FE3", "\\u03A5\\u0308\\u0301", "GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND OXIA"},
	{"\\u1FE4", "\\u03A1\\u0313", "GREEK SMALL LETTER RHO WITH PSILI"},
	{"\\u1FE6", "\\u03A5\\u0342", "GREEK SMALL LETTER UPSILON WITH PERISPOMENI"},
	{"\\u1FE7", "\\u03A5\\u0308\\u0342", "GREEK SMALL LETTER UPSILON WITH DIALYTIKA AND PERISPOMENI"},
	{"\\u1FF6", "\\u03A9\\u0342", "GREEK SMALL LETTER OMEGA WITH PERISPOMENI"},
	{"\\u1F80", "\\u1F08\\u0399", "GREEK SMALL LETTER ALPHA WITH PSILI AND YPOGEGRAMMENI"},
	{"\\u1F81", "\\u1F09\\u0399", "GREEK SMALL LETTER ALPHA WITH DASIA AND YPOGEGRAMMENI"},
	{"\\u1F82", "\\u1F0A\\u0399", "GREEK SMALL LETTER ALPHA WITH PSILI AND VARIA AND YPOGEGRAMMENI"},
	{"\\u1F83", "\\u1F0B\\u0399", "GREEK SMALL LETTER ALPHA WITH DASIA AND VARIA AND YPOGEGRAMMENI"},
	{"\\u1F84", "\\u1F0C\\u0399", "GREEK SMALL LETTER ALPHA WITH PSILI AND OXIA AND YPOGEGRAMMENI"},
	{"\\u1F85", "\\u1F0D\\u0399", "GREEK SMALL LETTER ALPHA WITH DASIA AND OXIA AND YPOGEGRAMMENI"},
	{"\\u1F86", "\\u1F0E\\u0399", "GREEK SMALL LETTER ALPHA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1F87", "\\u1F0F\\u0399", "GREEK SMALL LETTER ALPHA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1F88", "\\u1F08\\u0399", "GREEK CAPITAL LETTER ALPHA WITH PSILI AND PROSGEGRAMMENI"},
	{"\\u1F89", "\\u1F09\\u0399", "GREEK CAPITAL LETTER ALPHA WITH DASIA AND PROSGEGRAMMENI"},
	{"\\u1F8A", "\\u1F0A\\u0399", "GREEK CAPITAL LETTER ALPHA WITH PSILI AND VARIA AND PROSGEGRAMMENI"},
	{"\\u1F8B", "\\u1F0B\\u0399", "GREEK CAPITAL LETTER ALPHA WITH DASIA AND VARIA AND PROSGEGRAMMENI"},
	{"\\u1F8C", "\\u1F0C\\u0399", "GREEK CAPITAL LETTER ALPHA WITH PSILI AND OXIA AND PROSGEGRAMMENI"},
	{"\\u1F8D", "\\u1F0D\\u0399", "GREEK CAPITAL LETTER ALPHA WITH DASIA AND OXIA AND PROSGEGRAMMENI"},
	{"\\u1F8E", "\\u1F0E\\u0399", "GREEK CAPITAL LETTER ALPHA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI"},
	{"\\u1F8F", "\\u1F0F\\u0399", "GREEK CAPITAL LETTER ALPHA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI"},
	{"\\u1F90", "\\u1F28\\u0399", "GREEK SMALL LETTER ETA WITH PSILI AND YPOGEGRAMMENI"},
	{"\\u1F91", "\\u1F29\\u0399", "GREEK SMALL LETTER ETA WITH DASIA AND YPOGEGRAMMENI"},
	{"\\u1F92", "\\u1F2A\\u0399", "GREEK SMALL LETTER ETA WITH PSILI AND VARIA AND YPOGEGRAMMENI"},
	{"\\u1F93", "\\u1F2B\\u0399", "GREEK SMALL LETTER ETA WITH DASIA AND VARIA AND YPOGEGRAMMENI"},
	{"\\u1F94", "\\u1F2C\\u0399", "GREEK SMALL LETTER ETA WITH PSILI AND OXIA AND YPOGEGRAMMENI"},
	{"\\u1F95", "\\u1F2D\\u0399", "GREEK SMALL LETTER ETA WITH DASIA AND OXIA AND YPOGEGRAMMENI"},
	{"\\u1F96", "\\u1F2E\\u0399", "GREEK SMALL LETTER ETA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1F97", "\\u1F2F\\u0399", "GREEK SMALL LETTER ETA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1F98", "\\u1F28\\u0399", "GREEK CAPITAL LETTER ETA WITH PSILI AND PROSGEGRAMMENI"},
	{"\\u1F99", "\\u1F29\\u0399", "GREEK CAPITAL LETTER ETA WITH DASIA AND PROSGEGRAMMENI"},
	{"\\u1F9A", "\\u1F2A\\u0399", "GREEK CAPITAL LETTER ETA WITH PSILI AND VARIA AND PROSGEGRAMMENI"},
	{"\\u1F9B", "\\u1F2B\\u0399", "GREEK CAPITAL LETTER ETA WITH DASIA AND VARIA AND PROSGEGRAMMENI"},
	{"\\u1F9C", "\\u1F2C\\u0399", "GREEK CAPITAL LETTER ETA WITH PSILI AND OXIA AND PROSGEGRAMMENI"},
	{"\\u1F9D", "\\u1F2D\\u0399", "GREEK CAPITAL LETTER ETA WITH DASIA AND OXIA AND PROSGEGRAMMENI"},
	{"\\u1F9E", "\\u1F2E\\u0399", "GREEK CAPITAL LETTER ETA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI"},
	{"\\u1F9F", "\\u1F2F\\u0399", "GREEK CAPITAL LETTER ETA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI"},
	{"\\u1FA0", "\\u1F68\\u0399", "GREEK SMALL LETTER OMEGA WITH PSILI AND YPOGEGRAMMENI"},
	{"\\u1FA1", "\\u1F69\\u0399", "GREEK SMALL LETTER OMEGA WITH DASIA AND YPOGEGRAMMENI"},
	{"\\u1FA2", "\\u1F6A\\u0399", "GREEK SMALL LETTER OMEGA WITH PSILI AND VARIA AND YPOGEGRAMMENI"},
	{"\\u1FA3", "\\u1F6B\\u0399", "GREEK SMALL LETTER OMEGA WITH DASIA AND VARIA AND YPOGEGRAMMENI"},
	{"\\u1FA4", "\\u1F6C\\u0399", "GREEK SMALL LETTER OMEGA WITH PSILI AND OXIA AND YPOGEGRAMMENI"},
	{"\\u1FA5", "\\u1F6D\\u0399", "GREEK SMALL LETTER OMEGA WITH DASIA AND OXIA AND YPOGEGRAMMENI"},
	{"\\u1FA6", "\\u1F6E\\u0399", "GREEK SMALL LETTER OMEGA WITH PSILI AND PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1FA7", "\\u1F6F\\u0399", "GREEK SMALL LETTER OMEGA WITH DASIA AND PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1FA8", "\\u1F68\\u0399", "GREEK CAPITAL LETTER OMEGA WITH PSILI AND PROSGEGRAMMENI"},
	{"\\u1FA9", "\\u1F69\\u0399", "GREEK CAPITAL LETTER OMEGA WITH DASIA AND PROSGEGRAMMENI"},
	{"\\u1FAA", "\\u1F6A\\u0399", "GREEK CAPITAL LETTER OMEGA WITH PSILI AND VARIA AND PROSGEGRAMMENI"},
	{"\\u1FAB", "\\u1F6B\\u0399", "GREEK CAPITAL LETTER OMEGA WITH DASIA AND VARIA AND PROSGEGRAMMENI"},
	{"\\u1FAC", "\\u1F6C\\u0399", "GREEK CAPITAL LETTER OMEGA WITH PSILI AND OXIA AND PROSGEGRAMMENI"},
	{"\\u1FAD", "\\u1F6D\\u0399", "GREEK CAPITAL LETTER OMEGA WITH DASIA AND OXIA AND PROSGEGRAMMENI"},
	{"\\u1FAE", "\\u1F6E\\u0399", "GREEK CAPITAL LETTER OMEGA WITH PSILI AND PERISPOMENI AND PROSGEGRAMMENI"},
	{"\\u1FAF", "\\u1F6F\\u0399", "GREEK CAPITAL LETTER OMEGA WITH DASIA AND PERISPOMENI AND PROSGEGRAMMENI"},
	{"\\u1FB3", "\\u0391\\u0399", "GREEK SMALL LETTER ALPHA WITH YPOGEGRAMMENI"},
	{"\\u1FBC", "\\u0391\\u0399", "GREEK CAPITAL LETTER ALPHA WITH PROSGEGRAMMENI"},
	{"\\u1FC3", "\\u0397\\u0399", "GREEK SMALL LETTER ETA WITH YPOGEGRAMMENI"},
	{"\\u1FCC", "\\u0397\\u0399", "GREEK CAPITAL LETTER ETA WITH PROSGEGRAMMENI"},
	{"\\u1FF3", "\\u03A9\\u0399", "GREEK SMALL LETTER OMEGA WITH YPOGEGRAMMENI"},
	{"\\u1FFC", "\\u03A9\\u0399", "GREEK CAPITAL LETTER OMEGA WITH PROSGEGRAMMENI"},
	{"\\u1FB2", "\\u1FBA\\u0399", "GREEK SMALL LETTER ALPHA WITH VARIA AND YPOGEGRAMMENI"},
	{"\\u1FB4", "\\u0386\\u0399", "GREEK SMALL LETTER ALPHA WITH OXIA AND YPOGEGRAMMENI"},
	{"\\u1FC2", "\\u1FCA\\u0399", "GREEK SMALL LETTER ETA WITH VARIA AND YPOGEGRAMMENI"},
	{"\\u1FC4", "\\u0389\\u0399", "GREEK SMALL LETTER ETA WITH OXIA AND YPOGEGRAMMENI"},
	{"\\u1FF2", "\\u1FFA\\u0399", "GREEK SMALL LETTER OMEGA WITH VARIA AND YPOGEGRAMMENI"},
	{"\\u1FF4", "\\u038F\\u0399", "GREEK SMALL LETTER OMEGA WITH OXIA AND YPOGEGRAMMENI"},
	{"\\u1FB7", "\\u0391\\u0342\\u0399", "GREEK SMALL LETTER ALPHA WITH PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1FC7", "\\u0397\\u0342\\u0399", "GREEK SMALL LETTER ETA WITH PERISPOMENI AND YPOGEGRAMMENI"},
	{"\\u1FF7", "\\u03A9\\u0342\\u0399", "GREEK SMALL LETTER OMEGA WITH PERISPOMENI AND YPOGEGRAMMENI"},
};

int
main(void)
{
	size_t i;

	/*
	 * Generated from test262:
	 * test/built-ins/String/prototype/toUpperCase/special_casing.js
	 *
	 * This fixture keeps the upstream escaped code-unit inputs and expected
	 * outputs, then exercises `jsmethod_string_to_upper_case()` over the same
	 * locale-insensitive Unicode special-casing surface.
	 */

	for (i = 0; i < sizeof(special_cases) / sizeof(special_cases[0]); i++) {
		int rc = run_special_case(&special_cases[i]);
		if (rc != GENERATED_TEST_PASS) {
			return rc;
		}
	}

	return generated_test_pass(SUITE, CASE_NAME);
}

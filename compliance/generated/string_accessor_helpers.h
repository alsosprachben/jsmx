#ifndef JSMX_COMPLIANCE_STRING_ACCESSOR_HELPERS_H
#define JSMX_COMPLIANCE_STRING_ACCESSOR_HELPERS_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "compliance/generated/string_search_helpers.h"

static inline int
generated_expect_accessor_string(jsstr16_t *actual, const uint16_t *expected,
		size_t expected_len, const char *suite, const char *case_name,
		const char *label)
{
	size_t i;

	if (jsstr16_get_utf16len(actual) != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected length %zu, got %zu", label,
				expected_len, jsstr16_get_utf16len(actual));
	}
	for (i = 0; i < expected_len; i++) {
		if (actual->codeunits[i] != expected[i]) {
			return generated_test_fail(suite, case_name,
					"%s: expected code unit[%zu] = 0x%04x, got 0x%04x",
					label, i, (unsigned)expected[i],
					(unsigned)actual->codeunits[i]);
		}
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_accessor_number(double actual, double expected,
		const char *suite, const char *case_name, const char *label)
{
	if (actual != expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %.17g, got %.17g", label, expected, actual);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_accessor_nan(double actual, const char *suite,
		const char *case_name, const char *label)
{
	if (!isnan(actual)) {
		return generated_test_fail(suite, case_name,
				"%s: expected NaN, got %.17g", label, actual);
	}
	return GENERATED_TEST_PASS;
}

#endif

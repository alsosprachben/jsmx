#ifndef JSMX_COMPLIANCE_VALUE_HELPERS_H
#define JSMX_COMPLIANCE_VALUE_HELPERS_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

static inline jsval_t
generated_logical_and(jsval_region_t *region, jsval_t left, jsval_t right)
{
	return jsval_truthy(region, left) ? right : left;
}

static inline jsval_t
generated_logical_or(jsval_region_t *region, jsval_t left, jsval_t right)
{
	return jsval_truthy(region, left) ? left : right;
}

static inline int
generated_expect_value_string(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *suite,
		const char *case_name, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: failed to measure string result", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: failed to copy string result", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(suite, case_name,
				"%s: string result did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_value_strict_eq(jsval_region_t *region, jsval_t actual,
		jsval_t expected, const char *suite, const char *case_name,
		const char *label)
{
	if (jsval_strict_eq(region, actual, expected) != 1) {
		return generated_test_fail(suite, case_name,
				"%s: strict equality check failed", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_value_positive_zero(jsval_t value, const char *suite,
		const char *case_name, const char *label)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != 0.0
			|| signbit(value.as.number)) {
		return generated_test_fail(suite, case_name,
				"%s: expected +0 result", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_value_negative_zero(jsval_t value, const char *suite,
		const char *case_name, const char *label)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != 0.0
			|| !signbit(value.as.number)) {
		return generated_test_fail(suite, case_name,
				"%s: expected -0 result", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_value_nan(jsval_t value, const char *suite,
		const char *case_name, const char *label)
{
	if (value.kind != JSVAL_KIND_NUMBER || !isnan(value.as.number)) {
		return generated_test_fail(suite, case_name,
				"%s: expected NaN result", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_boolean_result(int actual, int expected, const char *suite,
		const char *case_name, const char *label)
{
	if (!!actual != !!expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %s result", label,
				expected ? "true" : "false");
	}
	return GENERATED_TEST_PASS;
}

#endif

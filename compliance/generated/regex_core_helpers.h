#ifndef JSMX_COMPLIANCE_REGEX_CORE_HELPERS_H
#define JSMX_COMPLIANCE_REGEX_CORE_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsregex.h"
#include "jsval.h"

typedef int (*generated_regexp_bool_accessor_t)(jsval_region_t *region,
		jsval_t regexp, int *result_ptr);

static inline int
generated_regexp_new_utf8(jsval_region_t *region, const char *pattern,
		const char *flags, jsval_t *value_ptr, jsmethod_error_t *error)
{
	jsval_t pattern_value;
	jsval_t flags_value = jsval_undefined();

	if (jsval_string_new_utf8(region, (const uint8_t *)pattern,
			strlen(pattern), &pattern_value) < 0) {
		return -1;
	}
	if (flags != NULL) {
		if (jsval_string_new_utf8(region, (const uint8_t *)flags,
				strlen(flags), &flags_value) < 0) {
			return -1;
		}
	}
	return jsval_regexp_new(region, pattern_value, flags != NULL, flags_value,
			value_ptr, error);
}

static inline int
generated_expect_regexp_source(jsval_region_t *region, jsval_t regexp,
		const char *expected, const char *suite, const char *case_name,
		const char *label)
{
	jsval_t source_value;
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_regexp_source(region, regexp, &source_value) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_regexp_source failed: %s", label, strerror(errno));
	}
	if (source_value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(suite, case_name,
				"%s: expected source string value", label);
	}
	if (jsval_string_copy_utf8(region, source_value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: source length failed: %s", label, strerror(errno));
	}
	if (actual_len != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected %zu bytes, got %zu", label,
				expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, source_value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: source copy failed: %s", label, strerror(errno));
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(suite, case_name,
				"%s: source bytes did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_regexp_flags(jsval_region_t *region, jsval_t regexp,
		const char *expected, const char *suite, const char *case_name,
		const char *label)
{
	jsval_t flags_value;
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_regexp_flags(region, regexp, &flags_value) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_regexp_flags failed: %s", label, strerror(errno));
	}
	if (flags_value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(suite, case_name,
				"%s: expected string flags value", label);
	}
	if (jsval_string_copy_utf8(region, flags_value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: string length failed: %s", label, strerror(errno));
	}
	if (actual_len != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected %zu bytes, got %zu", label,
				expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, flags_value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: string copy failed: %s", label, strerror(errno));
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(suite, case_name,
				"%s: string bytes did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_regexp_boolean(jsval_region_t *region, jsval_t regexp,
		generated_regexp_bool_accessor_t accessor, int expected,
		const char *suite, const char *case_name, const char *label)
{
	int actual = 0;

	if (accessor(region, regexp, &actual) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: boolean accessor failed: %s", label, strerror(errno));
	}
	if (actual != expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %d, got %d", label, expected, actual);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_regexp_info(jsval_region_t *region, jsval_t regexp,
		uint32_t expected_flags, uint32_t expected_capture_count,
		size_t expected_last_index, const char *suite,
		const char *case_name, const char *label)
{
	jsval_regexp_info_t info;

	if (jsval_regexp_info(region, regexp, &info) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_regexp_info failed: %s", label, strerror(errno));
	}
	if (info.flags != expected_flags) {
		return generated_test_fail(suite, case_name,
				"%s: expected flags mask 0x%x, got 0x%x", label,
				expected_flags, info.flags);
	}
	if (info.capture_count != expected_capture_count) {
		return generated_test_fail(suite, case_name,
				"%s: expected capture_count %u, got %u", label,
				expected_capture_count, info.capture_count);
	}
	if (info.last_index != expected_last_index) {
		return generated_test_fail(suite, case_name,
				"%s: expected lastIndex %zu, got %zu", label,
				expected_last_index, info.last_index);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_regexp_has_flag(jsval_region_t *region, jsval_t regexp,
		uint32_t flag, int expected, const char *suite,
		const char *case_name, const char *label)
{
	jsval_regexp_info_t info;
	int actual;

	if (jsval_regexp_info(region, regexp, &info) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_regexp_info failed: %s", label, strerror(errno));
	}
	actual = (info.flags & flag) != 0;
	if (actual != expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %d, got %d", label, expected, actual);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_regexp_test(jsval_region_t *region, jsval_t regexp,
		jsval_t input_value, int expected, const char *suite,
		const char *case_name, const char *label)
{
	jsmethod_error_t error;
	int actual = 0;

	if (jsval_regexp_test(region, regexp, input_value, &actual, &error) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: jsval_regexp_test failed: errno=%d kind=%d", label,
				errno, (int)error.kind);
	}
	if (actual != expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %d, got %d", label, expected, actual);
	}
	return GENERATED_TEST_PASS;
}

#endif

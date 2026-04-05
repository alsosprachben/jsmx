#ifndef JSMX_COMPLIANCE_REGEX_EXEC_MATCH_HELPERS_H
#define JSMX_COMPLIANCE_REGEX_EXEC_MATCH_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

static inline int
generated_expect_match_string_prop(jsval_region_t *region, jsval_t object,
		const char *key, const char *expected, const char *suite,
		const char *case_name, const char *label)
{
	jsval_t value;
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (object.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected object-like match result", label);
	}
	if (jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: property lookup failed: %s", label, strerror(errno));
	}
	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(suite, case_name,
				"%s: expected string property", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: string length failed: %s", label, strerror(errno));
	}
	if (actual_len != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected %zu bytes, got %zu", label,
				expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
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
generated_expect_match_number_prop(jsval_region_t *region, jsval_t object,
		const char *key, double expected, const char *suite,
		const char *case_name, const char *label)
{
	jsval_t value;

	if (object.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected object-like match result", label);
	}
	if (jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: property lookup failed: %s", label, strerror(errno));
	}
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != expected) {
		return generated_test_fail(suite, case_name,
				"%s: expected %.17g, got %s%.17g", label, expected,
				value.kind == JSVAL_KIND_NUMBER ? "" : "non-number/",
				value.kind == JSVAL_KIND_NUMBER ? value.as.number : 0.0);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_match_undefined_prop(jsval_region_t *region, jsval_t object,
		const char *key, const char *suite, const char *case_name,
		const char *label)
{
	jsval_t value;

	if (object.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected object-like match result", label);
	}
	if (jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: property lookup failed: %s", label, strerror(errno));
	}
	if (value.kind != JSVAL_KIND_UNDEFINED) {
		return generated_test_fail(suite, case_name,
				"%s: expected undefined property", label);
	}
	return GENERATED_TEST_PASS;
}

static inline int
generated_expect_match_groups_string_prop(jsval_region_t *region,
		jsval_t object, const char *group_key, const char *expected,
		const char *suite, const char *case_name, const char *label)
{
	jsval_t groups;

	if (object.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected object-like match result", label);
	}
	if (jsval_object_get_utf8(region, object, (const uint8_t *)"groups", 6,
			&groups) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: groups lookup failed: %s", label, strerror(errno));
	}
	if (groups.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected groups object", label);
	}
	return generated_expect_match_string_prop(region, groups, group_key, expected,
			suite, case_name, label);
}

static inline int
generated_expect_match_groups_undefined_prop(jsval_region_t *region,
		jsval_t object, const char *group_key, const char *suite,
		const char *case_name, const char *label)
{
	jsval_t groups;

	if (object.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected object-like match result", label);
	}
	if (jsval_object_get_utf8(region, object, (const uint8_t *)"groups", 6,
			&groups) < 0) {
		return generated_test_fail(suite, case_name,
				"%s: groups lookup failed: %s", label, strerror(errno));
	}
	if (groups.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(suite, case_name,
				"%s: expected groups object", label);
	}
	return generated_expect_match_undefined_prop(region, groups, group_key,
			suite, case_name, label);
}

static inline int
generated_expect_match_array_strings(jsval_region_t *region, jsval_t array,
		const char *const *expected, size_t expected_len,
		const char *suite, const char *case_name, const char *label)
{
	size_t i;

	if (array.kind != JSVAL_KIND_ARRAY) {
		return generated_test_fail(suite, case_name,
				"%s: expected array result", label);
	}
	if (jsval_array_length(region, array) != expected_len) {
		return generated_test_fail(suite, case_name,
				"%s: expected length %zu, got %zu", label, expected_len,
				jsval_array_length(region, array));
	}
	for (i = 0; i < expected_len; i++) {
		jsval_t value;

		if (jsval_array_get(region, array, i, &value) < 0) {
			return generated_test_fail(suite, case_name,
					"%s[%zu]: array get failed: %s", label, i,
					strerror(errno));
		}
		if (value.kind != JSVAL_KIND_STRING) {
			return generated_test_fail(suite, case_name,
					"%s[%zu]: expected string element", label, i);
		}
		{
			size_t expected_item_len = strlen(expected[i]);
			size_t actual_len = 0;
			uint8_t buf[expected_item_len ? expected_item_len : 1];

			if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: string length failed: %s", label, i,
						strerror(errno));
			}
			if (actual_len != expected_item_len) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: expected %zu bytes, got %zu", label, i,
						expected_item_len, actual_len);
			}
			if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: string copy failed: %s", label, i,
						strerror(errno));
			}
			if (memcmp(buf, expected[i], expected_item_len) != 0) {
				return generated_test_fail(suite, case_name,
						"%s[%zu]: string bytes did not match expected output",
						label, i);
			}
		}
	}
	return GENERATED_TEST_PASS;
}

#endif

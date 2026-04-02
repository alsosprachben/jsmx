#ifndef JSMX_COMPLIANCE_STRING_REPLACE_HELPERS_H
#define JSMX_COMPLIANCE_STRING_REPLACE_HELPERS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/string_pad_helpers.h"
#include "jsval.h"

#define GENERATED_LEN(a) (sizeof(a) / sizeof((a)[0]))

static inline int
generated_measure_and_replace(jsstr16_t *out, uint16_t *storage,
		size_t storage_cap, jsmethod_value_t this_value,
		jsmethod_value_t search_value, jsmethod_value_t replacement_value,
		jsmethod_error_t *error)
{
	jsmethod_string_replace_sizes_t sizes;

	if (jsmethod_string_replace_measure(this_value, search_value,
			replacement_value, &sizes, error) < 0) {
		return -1;
	}
	if (sizes.result_len > storage_cap) {
		errno = ENOBUFS;
		return -1;
	}
	jsstr16_init_from_buf(out, (char *)storage,
			storage_cap * sizeof(storage[0]));
	return jsmethod_string_replace(out, this_value, search_value,
			replacement_value, error);
}

static inline int
generated_measure_and_replace_all(jsstr16_t *out, uint16_t *storage,
		size_t storage_cap, jsmethod_value_t this_value,
		jsmethod_value_t search_value, jsmethod_value_t replacement_value,
		jsmethod_error_t *error)
{
	jsmethod_string_replace_sizes_t sizes;

	if (jsmethod_string_replace_all_measure(this_value, search_value,
			replacement_value, &sizes, error) < 0) {
		return -1;
	}
	if (sizes.result_len > storage_cap) {
		errno = ENOBUFS;
		return -1;
	}
	jsstr16_init_from_buf(out, (char *)storage,
			storage_cap * sizeof(storage[0]));
	return jsmethod_string_replace_all(out, this_value, search_value,
			replacement_value, error);
}

static inline int
generated_expect_jsval_utf8_string(jsval_region_t *region, jsval_t value,
		const char *expected, const char *suite, const char *case_name,
		const char *label)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (region == NULL) {
		return generated_test_fail(suite, case_name,
				"%s: missing jsval region", label);
	}
	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(suite, case_name,
				"%s: expected string result", label);
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

#endif

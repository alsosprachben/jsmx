#ifndef JSMX_COMPLIANCE_STRING_PAD_HELPERS_H
#define JSMX_COMPLIANCE_STRING_PAD_HELPERS_H

#include <errno.h>
#include <string.h>

#include "compliance/generated/string_trim_repeat_helpers.h"

typedef int (*generated_jsmethod_pad_measure_fn)(jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes, jsmethod_error_t *error);

typedef int (*generated_jsmethod_pad_fn)(jsstr16_t *out,
		jsmethod_value_t this_value, int have_max_length,
		jsmethod_value_t max_length_value, int have_fill_string,
		jsmethod_value_t fill_string_value, jsmethod_error_t *error);

static inline int
generated_measure_and_pad(jsstr16_t *out, uint16_t *storage,
		size_t storage_cap, jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		generated_jsmethod_pad_measure_fn measure_fn,
		generated_jsmethod_pad_fn fn, jsmethod_error_t *error)
{
	jsmethod_string_pad_sizes_t sizes;

	if (measure_fn(this_value, have_max_length, max_length_value,
			have_fill_string, fill_string_value, &sizes, error) < 0) {
		return -1;
	}
	if (sizes.result_len > storage_cap) {
		errno = ENOBUFS;
		return -1;
	}
	jsstr16_init_from_buf(out, (char *)storage,
			storage_cap * sizeof(storage[0]));
	return fn(out, this_value, have_max_length, max_length_value,
			have_fill_string, fill_string_value, error);
}

static inline int
generated_expect_utf8_string(jsstr16_t *actual, const char *expected,
		const char *suite, const char *case_name, const char *label)
{
	size_t len = strlen(expected);
	uint16_t storage[len ? len : 1];
	jsstr16_t want;

	jsstr16_init_from_buf(&want, (char *)storage,
			sizeof(storage));
	if (jsstr16_set_from_utf8(&want, (const uint8_t *)expected, len) != len) {
		return generated_test_fail(suite, case_name,
				"%s: failed to build UTF-16 expectation", label);
	}
	return generated_expect_accessor_string(actual, want.codeunits,
			want.len, suite, case_name, label);
}

#endif

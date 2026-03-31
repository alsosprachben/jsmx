#ifndef JSMX_COMPLIANCE_STRING_TRIM_REPEAT_HELPERS_H
#define JSMX_COMPLIANCE_STRING_TRIM_REPEAT_HELPERS_H

#include <errno.h>
#include <string.h>

#include "compliance/generated/string_accessor_helpers.h"

#define GENERATED_LEN(a) (sizeof(a) / sizeof((a)[0]))

static const uint16_t generated_trim_whitespace[] = {
	0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020, 0x00A0, 0x1680,
	0x2000, 0x2001, 0x2002, 0x2003, 0x2004, 0x2005, 0x2006, 0x2007,
	0x2008, 0x2009, 0x200A, 0x202F, 0x205F, 0x3000, 0x2028, 0x2029,
	0xFEFF
};

static const uint16_t generated_trim_line_terminators[] = {
	0x000A, 0x000D, 0x2028, 0x2029
};

static inline int
generated_expect_empty_string(jsstr16_t *actual, const char *suite,
		const char *case_name, const char *label)
{
	return generated_expect_accessor_string(actual, NULL, 0, suite, case_name,
			label);
}

static inline int
generated_measure_and_repeat(jsstr16_t *out, uint16_t *storage,
		size_t storage_cap, jsmethod_value_t this_value, int have_count,
		jsmethod_value_t count_value, jsmethod_error_t *error)
{
	jsmethod_string_repeat_sizes_t sizes;

	if (jsmethod_string_repeat_measure(this_value, have_count, count_value,
			&sizes, error) < 0) {
		return -1;
	}
	if (sizes.result_len > storage_cap) {
		errno = ENOBUFS;
		return -1;
	}
	jsstr16_init_from_buf(out, (char *)storage,
			storage_cap * sizeof(storage[0]));
	return jsmethod_string_repeat(out, this_value, have_count, count_value,
			error);
}

#endif

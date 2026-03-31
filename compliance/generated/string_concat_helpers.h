#ifndef JSMX_COMPLIANCE_STRING_CONCAT_HELPERS_H
#define JSMX_COMPLIANCE_STRING_CONCAT_HELPERS_H

#include <errno.h>

#include "compliance/generated/string_pad_helpers.h"

static inline int
generated_measure_and_concat(jsstr16_t *out, uint16_t *storage,
		size_t storage_cap, jsmethod_value_t this_value, size_t arg_count,
		const jsmethod_value_t *args, jsmethod_error_t *error)
{
	jsmethod_string_concat_sizes_t sizes;

	if (jsmethod_string_concat_measure(this_value, arg_count, args, &sizes,
			error) < 0) {
		return -1;
	}
	if (sizes.result_len > storage_cap) {
		errno = ENOBUFS;
		return -1;
	}
	jsstr16_init_from_buf(out, (char *)storage,
			storage_cap * sizeof(storage[0]));
	return jsmethod_string_concat(out, this_value, arg_count, args, error);
}

#endif

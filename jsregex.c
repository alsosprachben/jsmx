#include "jsregex.h"

#if JSMX_WITH_REGEX

#include <errno.h>
#include <string.h>

#if JSMX_REGEX_BACKEND_PCRE2

#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>

typedef struct jsregex_options_s {
	uint32_t compile_options;
	uint32_t flags;
} jsregex_options_t;

static int
jsregex_is_high_surrogate(uint16_t unit)
{
	return unit >= 0xD800 && unit <= 0xDBFF;
}

static int
jsregex_is_low_surrogate(uint16_t unit)
{
	return unit >= 0xDC00 && unit <= 0xDFFF;
}

static int
jsregex_parse_flags(const uint16_t *flags, size_t flags_len,
		jsregex_options_t *options_ptr)
{
	jsregex_options_t options = {0};
	int seen_g = 0;
	int seen_i = 0;
	int seen_m = 0;
	int seen_s = 0;
	int seen_u = 0;
	int seen_y = 0;
	size_t i;

	if (options_ptr == NULL || (flags_len > 0 && flags == NULL)) {
		errno = EINVAL;
		return -1;
	}

	for (i = 0; i < flags_len; i++) {
		switch (flags[i]) {
		case 'g':
			if (seen_g) {
				errno = EINVAL;
				return -1;
			}
			seen_g = 1;
			options.flags |= JSREGEX_FLAG_GLOBAL;
			break;
		case 'i':
			if (seen_i) {
				errno = EINVAL;
				return -1;
			}
			seen_i = 1;
			options.flags |= JSREGEX_FLAG_IGNORE_CASE;
			options.compile_options |= PCRE2_CASELESS;
			break;
		case 'm':
			if (seen_m) {
				errno = EINVAL;
				return -1;
			}
			seen_m = 1;
			options.flags |= JSREGEX_FLAG_MULTILINE;
			options.compile_options |= PCRE2_MULTILINE;
			break;
		case 's':
			if (seen_s) {
				errno = EINVAL;
				return -1;
			}
			seen_s = 1;
			options.flags |= JSREGEX_FLAG_DOT_ALL;
			options.compile_options |= PCRE2_DOTALL;
			break;
		case 'u':
			if (seen_u) {
				errno = EINVAL;
				return -1;
			}
			seen_u = 1;
			options.flags |= JSREGEX_FLAG_UNICODE;
			options.compile_options |= PCRE2_UTF | PCRE2_UCP;
			break;
		case 'y':
			if (seen_y) {
				errno = EINVAL;
				return -1;
			}
			seen_y = 1;
			options.flags |= JSREGEX_FLAG_STICKY;
			break;
		default:
			errno = ENOTSUP;
			return -1;
		}
	}

	*options_ptr = options;
	return 0;
}

int
jsregex_compile_utf16(const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_compiled_t *compiled_ptr)
{
	jsregex_options_t options;
	pcre2_code *code = NULL;
	int error_code;
	PCRE2_SIZE error_offset;
	uint32_t capture_count = 0;

	if ((pattern_len > 0 && pattern == NULL) || compiled_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	memset(compiled_ptr, 0, sizeof(*compiled_ptr));
	if (jsregex_parse_flags(flags, flags_len, &options) < 0) {
		return -1;
	}

	code = pcre2_compile((PCRE2_SPTR16)pattern, pattern_len,
			options.compile_options, &error_code, &error_offset, NULL);
	if (code == NULL) {
		(void)error_code;
		(void)error_offset;
		errno = EINVAL;
		return -1;
	}
	if (pcre2_pattern_info(code, PCRE2_INFO_CAPTURECOUNT, &capture_count) != 0) {
		pcre2_code_free(code);
		errno = EINVAL;
		return -1;
	}

	compiled_ptr->backend_code = (uintptr_t)code;
	compiled_ptr->flags = options.flags;
	compiled_ptr->capture_count = capture_count;
	return 0;
}

void
jsregex_release(jsregex_compiled_t *compiled_ptr)
{
	pcre2_code *code;

	if (compiled_ptr == NULL || compiled_ptr->backend_code == 0) {
		return;
	}
	code = (pcre2_code *)(uintptr_t)compiled_ptr->backend_code;
	pcre2_code_free(code);
	compiled_ptr->backend_code = 0;
	compiled_ptr->flags = 0;
	compiled_ptr->capture_count = 0;
}

int
jsregex_exec_utf16(const jsregex_compiled_t *compiled,
		const uint16_t *subject, size_t subject_len, size_t start_index,
		size_t *offsets, size_t offsets_cap,
		jsregex_exec_result_t *result_ptr)
{
	pcre2_code *code;
	pcre2_match_data *match_data = NULL;
	PCRE2_SIZE *ovector;
	uint32_t match_options = 0;
	size_t slot_count;
	size_t i;
	int rc;

	if (compiled == NULL || compiled->backend_code == 0
			|| result_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (start_index > subject_len) {
		result_ptr->matched = 0;
		result_ptr->start = 0;
		result_ptr->end = 0;
		result_ptr->slot_count = 0;
		return 0;
	}

	slot_count = (size_t)compiled->capture_count + 1;
	if (offsets == NULL || offsets_cap < slot_count * 2) {
		errno = ENOBUFS;
		return -1;
	}

	code = (pcre2_code *)(uintptr_t)compiled->backend_code;
	match_data = pcre2_match_data_create_from_pattern(code, NULL);
	if (match_data == NULL) {
		errno = ENOMEM;
		return -1;
	}
	if ((compiled->flags & JSREGEX_FLAG_STICKY) != 0) {
		match_options |= PCRE2_ANCHORED;
	}

	rc = pcre2_match(code, (PCRE2_SPTR16)subject, subject_len, start_index,
			match_options, match_data, NULL);
	if (rc == PCRE2_ERROR_NOMATCH) {
		result_ptr->matched = 0;
		result_ptr->start = 0;
		result_ptr->end = 0;
		result_ptr->slot_count = 0;
		pcre2_match_data_free(match_data);
		return 0;
	}
	if (rc < 0) {
		pcre2_match_data_free(match_data);
		errno = EINVAL;
		return -1;
	}

	ovector = pcre2_get_ovector_pointer(match_data);
	for (i = 0; i < slot_count; i++) {
		PCRE2_SIZE start = ovector[i * 2];
		PCRE2_SIZE end = ovector[i * 2 + 1];

		if (start == PCRE2_UNSET || end == PCRE2_UNSET) {
			offsets[i * 2] = SIZE_MAX;
			offsets[i * 2 + 1] = SIZE_MAX;
		} else {
			offsets[i * 2] = (size_t)start;
			offsets[i * 2 + 1] = (size_t)end;
		}
	}

	result_ptr->matched = 1;
	result_ptr->start = offsets[0];
	result_ptr->end = offsets[1];
	result_ptr->slot_count = slot_count;
	pcre2_match_data_free(match_data);
	return 0;
}

int
jsregex_exec_u_literal_surrogate_utf16(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t start_index,
		jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (!jsregex_is_high_surrogate(surrogate_unit)
			&& !jsregex_is_low_surrogate(surrogate_unit)) {
		errno = EINVAL;
		return -1;
	}
	if (start_index > subject_len) {
		result_ptr->matched = 0;
		result_ptr->start = 0;
		result_ptr->end = 0;
		result_ptr->slot_count = 0;
		return 0;
	}

	index = start_index;
	while (index < subject_len) {
		uint16_t unit = subject[index];

		if (unit == surrogate_unit) {
			int isolated = 0;

			if (jsregex_is_high_surrogate(surrogate_unit)) {
				isolated = !(index + 1 < subject_len
						&& jsregex_is_low_surrogate(subject[index + 1]));
			} else {
				isolated = !(index > 0
						&& jsregex_is_high_surrogate(subject[index - 1]));
			}
			if (isolated) {
				result_ptr->matched = 1;
				result_ptr->start = index;
				result_ptr->end = index + 1;
				result_ptr->slot_count = 1;
				return 0;
			}
		}

		if (jsregex_advance_string_index_utf16(subject, subject_len, index, 1,
				&index) < 0) {
			return -1;
		}
	}

	result_ptr->matched = 0;
	result_ptr->start = 0;
	result_ptr->end = 0;
	result_ptr->slot_count = 0;
	return 0;
}

int
jsregex_test_u_literal_surrogate_utf16(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t start_index,
		int *matched_ptr)
{
	jsregex_exec_result_t result;

	if (matched_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_surrogate_utf16(subject, subject_len,
			surrogate_unit, start_index, &result) < 0) {
		return -1;
	}
	*matched_ptr = result.matched;
	return 0;
}

int
jsregex_search_u_literal_surrogate_utf16(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t start_index,
		jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_surrogate_utf16(subject, subject_len,
			surrogate_unit, start_index, &result) < 0) {
		return -1;
	}
	result_ptr->matched = result.matched;
	result_ptr->start = result.start;
	result_ptr->end = result.end;
	return 0;
}

int
jsregex_search_utf16(const uint16_t *subject, size_t subject_len,
		const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_search_result_t *result_ptr)
{
	jsregex_compiled_t compiled;
	jsregex_exec_result_t exec_result;
	int rc;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_compile_utf16(pattern, pattern_len, flags, flags_len,
			&compiled) < 0) {
		return -1;
	}
	{
		size_t slot_count = (size_t)compiled.capture_count + 1;
		size_t slot_offsets[slot_count * 2];

		rc = jsregex_exec_utf16(&compiled, subject, subject_len, 0,
				slot_offsets, slot_count * 2, &exec_result);
		jsregex_release(&compiled);
		if (rc < 0) {
			return -1;
		}
	}

	result_ptr->matched = exec_result.matched;
	result_ptr->start = exec_result.start;
	result_ptr->end = exec_result.end;
	return 0;
}

int
jsregex_advance_string_index_utf16(const uint16_t *subject,
		size_t subject_len, size_t index, int unicode,
		size_t *next_index_ptr)
{
	uint16_t first;

	if (next_index_ptr == NULL || (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (index >= subject_len) {
		*next_index_ptr = index + 1;
		return 0;
	}
	if (!unicode) {
		*next_index_ptr = index + 1;
		return 0;
	}

	first = subject[index];
	if (first >= 0xD800 && first <= 0xDBFF && index + 1 < subject_len) {
		uint16_t second = subject[index + 1];

		if (second >= 0xDC00 && second <= 0xDFFF) {
			*next_index_ptr = index + 2;
			return 0;
		}
	}

	*next_index_ptr = index + 1;
	return 0;
}

#else

#error "JSMX_WITH_REGEX requires a configured regex backend"

#endif

#endif

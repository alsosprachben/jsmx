#include "jsregex.h"

#if JSMX_WITH_REGEX

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if JSMX_REGEX_BACKEND_PCRE2

#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>

typedef struct jsregex_options_s {
	uint32_t compile_options;
	uint32_t flags;
} jsregex_options_t;

typedef struct jsregex_backend_s {
	pcre2_code *code;
	pcre2_match_data *match_data;
	uint8_t use_match_data_cache;
} jsregex_backend_t;

static int
jsregex_compile_utf16_impl(const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len, int use_jit,
		jsregex_compiled_t *compiled_ptr);

static jsregex_backend_t *
jsregex_backend(const jsregex_compiled_t *compiled)
{
	if (compiled == NULL || compiled->backend_code == 0) {
		return NULL;
	}
	return (jsregex_backend_t *)(uintptr_t)compiled->backend_code;
}

static pcre2_code *
jsregex_backend_code(const jsregex_compiled_t *compiled)
{
	jsregex_backend_t *backend = jsregex_backend(compiled);

	return backend != NULL ? backend->code : NULL;
}

static pcre2_match_data *
jsregex_backend_match_data(jsregex_backend_t *backend)
{
	if (backend == NULL || backend->code == NULL) {
		errno = EINVAL;
		return NULL;
	}
	if (backend->match_data == NULL) {
		backend->match_data = pcre2_match_data_create_from_pattern(
				backend->code, NULL);
		if (backend->match_data == NULL) {
			errno = ENOMEM;
			return NULL;
		}
	}
	return backend->match_data;
}

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
jsregex_is_code_point_boundary(const uint16_t *subject, size_t subject_len,
		size_t index)
{
	if (index > subject_len) {
		return 0;
	}
	if (index == 0 || index == subject_len) {
		return 1;
	}
	return !(jsregex_is_low_surrogate(subject[index])
			&& jsregex_is_high_surrogate(subject[index - 1]));
}

static int
jsregex_u_literal_class_match_at(const uint16_t *subject, size_t subject_len,
		const uint16_t *members, size_t members_len, size_t index,
		int negate, int *matched_ptr, size_t *end_ptr)
{
	uint16_t unit;
	size_t i;
	int member_found = 0;

	if (matched_ptr == NULL || end_ptr == NULL || members == NULL
			|| members_len == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (index >= subject_len
			|| !jsregex_is_code_point_boundary(subject, subject_len,
				index)) {
		*matched_ptr = 0;
		*end_ptr = index;
		return 0;
	}

	unit = subject[index];
	if (jsregex_is_high_surrogate(unit) && index + 1 < subject_len
			&& jsregex_is_low_surrogate(subject[index + 1])) {
		*matched_ptr = 0;
		*end_ptr = index + 2;
		return 0;
	}

	for (i = 0; i < members_len; i++) {
		if (members[i] == unit) {
			member_found = 1;
			break;
		}
	}

	*matched_ptr = negate ? !member_found : member_found;
	*end_ptr = index + 1;
	return 0;
}

static int
jsregex_u_literal_range_class_match_at(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t index, int negate, int *matched_ptr, size_t *end_ptr)
{
	uint16_t unit;
	size_t i;
	int range_found = 0;

	if (matched_ptr == NULL || end_ptr == NULL || ranges == NULL
			|| range_count == 0
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (index >= subject_len
			|| !jsregex_is_code_point_boundary(subject, subject_len,
				index)) {
		*matched_ptr = 0;
		*end_ptr = index;
		return 0;
	}

	unit = subject[index];
	if (jsregex_is_high_surrogate(unit) && index + 1 < subject_len
			&& jsregex_is_low_surrogate(subject[index + 1])) {
		*matched_ptr = 0;
		*end_ptr = index + 2;
		return 0;
	}

	for (i = 0; i < range_count; i++) {
		uint16_t start = ranges[i * 2];
		uint16_t end = ranges[i * 2 + 1];

		if (start > end) {
			errno = EINVAL;
			return -1;
		}
		if (unit >= start && unit <= end) {
			range_found = 1;
			break;
		}
	}

	*matched_ptr = negate ? !range_found : range_found;
	*end_ptr = index + 1;
	return 0;
}

static int
jsregex_is_ecma_whitespace_or_line_terminator(uint16_t unit)
{
	switch (unit) {
	case 0x0009:
	case 0x000A:
	case 0x000B:
	case 0x000C:
	case 0x000D:
	case 0x0020:
	case 0x00A0:
	case 0x1680:
	case 0x2028:
	case 0x2029:
	case 0x202F:
	case 0x205F:
	case 0x3000:
	case 0xFEFF:
		return 1;
	default:
		return unit >= 0x2000 && unit <= 0x200A;
	}
}

static int
jsregex_u_predefined_class_matches_unit(uint16_t unit,
		jsregex_u_predefined_class_kind_t kind, int *matched_ptr)
{
	int positive_match;

	if (matched_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}

	switch (kind) {
	case JSREGEX_U_PREDEFINED_CLASS_DIGIT:
		positive_match = unit >= '0' && unit <= '9';
		break;
	case JSREGEX_U_PREDEFINED_CLASS_NOT_DIGIT:
		positive_match = !(unit >= '0' && unit <= '9');
		break;
	case JSREGEX_U_PREDEFINED_CLASS_WHITESPACE:
		positive_match = jsregex_is_ecma_whitespace_or_line_terminator(unit);
		break;
	case JSREGEX_U_PREDEFINED_CLASS_NOT_WHITESPACE:
		positive_match = !jsregex_is_ecma_whitespace_or_line_terminator(unit);
		break;
	case JSREGEX_U_PREDEFINED_CLASS_WORD:
		positive_match = (unit >= 'A' && unit <= 'Z')
				|| (unit >= 'a' && unit <= 'z')
				|| (unit >= '0' && unit <= '9')
				|| unit == '_';
		break;
	case JSREGEX_U_PREDEFINED_CLASS_NOT_WORD:
		positive_match = !((unit >= 'A' && unit <= 'Z')
				|| (unit >= 'a' && unit <= 'z')
				|| (unit >= '0' && unit <= '9')
				|| unit == '_');
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	*matched_ptr = positive_match;
	return 0;
}

static int
jsregex_u_predefined_class_match_at(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		size_t index, int *matched_ptr, size_t *end_ptr)
{
	uint16_t unit;

	if (matched_ptr == NULL || end_ptr == NULL
			|| (subject_len > 0 && subject == NULL)) {
		errno = EINVAL;
		return -1;
	}
	if (index >= subject_len
			|| !jsregex_is_code_point_boundary(subject, subject_len,
				index)) {
		*matched_ptr = 0;
		*end_ptr = index;
		return 0;
	}

	unit = subject[index];
	if (jsregex_is_high_surrogate(unit) && index + 1 < subject_len
			&& jsregex_is_low_surrogate(subject[index + 1])) {
		*matched_ptr = 0;
		*end_ptr = index + 2;
		return 0;
	}

	if (jsregex_u_predefined_class_matches_unit(unit, kind, matched_ptr) < 0) {
		return -1;
	}
	*end_ptr = index + 1;
	return 0;
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
jsregex_compile_utf16_impl(const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		int use_jit, jsregex_compiled_t *compiled_ptr)
{
	jsregex_options_t options;
	jsregex_backend_t *backend = NULL;
	pcre2_code *code = NULL;
	int error_code;
	PCRE2_SIZE error_offset;
	uint32_t capture_count = 0;
	uint32_t named_group_count = 0;

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
	if (pcre2_pattern_info(code, PCRE2_INFO_NAMECOUNT,
			&named_group_count) != 0) {
		pcre2_code_free(code);
		errno = EINVAL;
		return -1;
	}
	if (use_jit) {
		/* Treat JIT as an opportunistic backend optimization only. If the
		 * linked PCRE2 does not support JIT for this pattern or target, keep
		 * the compiled pattern and fall back to the normal pcre2_match path.
		 */
		(void)pcre2_jit_compile(code, PCRE2_JIT_COMPLETE);
	}

	backend = (jsregex_backend_t *)malloc(sizeof(*backend));
	if (backend == NULL) {
		pcre2_code_free(code);
		errno = ENOMEM;
		return -1;
	}
	backend->code = code;
	backend->match_data = NULL;
	backend->use_match_data_cache = use_jit ? 1u : 0u;

	compiled_ptr->backend_code = (uintptr_t)backend;
	compiled_ptr->flags = options.flags;
	compiled_ptr->capture_count = capture_count;
	compiled_ptr->named_group_count = named_group_count;
	return 0;
}

int
jsregex_compile_utf16(const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_compiled_t *compiled_ptr)
{
	return jsregex_compile_utf16_impl(pattern, pattern_len, flags, flags_len,
			0, compiled_ptr);
}

int
jsregex_compile_utf16_jit(const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_compiled_t *compiled_ptr)
{
	return jsregex_compile_utf16_impl(pattern, pattern_len, flags, flags_len,
			1, compiled_ptr);
}

void
jsregex_release(jsregex_compiled_t *compiled_ptr)
{
	jsregex_backend_t *backend;

	if (compiled_ptr == NULL || compiled_ptr->backend_code == 0) {
		return;
	}
	backend = jsregex_backend(compiled_ptr);
	if (backend != NULL) {
		if (backend->match_data != NULL) {
			pcre2_match_data_free(backend->match_data);
		}
		if (backend->code != NULL) {
			pcre2_code_free(backend->code);
		}
		free(backend);
	}
	compiled_ptr->backend_code = 0;
	compiled_ptr->flags = 0;
	compiled_ptr->capture_count = 0;
	compiled_ptr->named_group_count = 0;
}

int
jsregex_exec_utf16(const jsregex_compiled_t *compiled,
		const uint16_t *subject, size_t subject_len, size_t start_index,
		size_t *offsets, size_t offsets_cap,
		jsregex_exec_result_t *result_ptr)
{
	jsregex_backend_t *backend;
	pcre2_code *code;
	pcre2_match_data *match_data;
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

	backend = jsregex_backend(compiled);
	code = jsregex_backend_code(compiled);
	if (code == NULL || backend == NULL) {
		if (errno == 0) {
			errno = EINVAL;
		}
		return -1;
	}
	if (backend->use_match_data_cache) {
		match_data = jsregex_backend_match_data(backend);
		if (match_data == NULL) {
			if (errno == 0) {
				errno = ENOMEM;
			}
			return -1;
		}
	} else {
		match_data = pcre2_match_data_create_from_pattern(code, NULL);
		if (match_data == NULL) {
			errno = ENOMEM;
			return -1;
		}
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
		if (!backend->use_match_data_cache) {
			pcre2_match_data_free(match_data);
		}
		return 0;
	}
	if (rc < 0) {
		if (!backend->use_match_data_cache) {
			pcre2_match_data_free(match_data);
		}
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
	if (!backend->use_match_data_cache) {
		pcre2_match_data_free(match_data);
	}
	return 0;
}

int
jsregex_named_group_utf16(const jsregex_compiled_t *compiled,
		size_t index, uint32_t *capture_index_ptr, uint16_t *name_buf,
		size_t name_cap, size_t *name_len_ptr)
{
	pcre2_code *code;
	uint32_t name_count = 0;
	uint32_t name_entry_size = 0;
	PCRE2_SPTR16 name_table = NULL;
	PCRE2_SPTR16 entry;
	size_t name_len = 0;

	if (compiled == NULL || compiled->backend_code == 0 || index >= SIZE_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (index >= compiled->named_group_count) {
		errno = EINVAL;
		return -1;
	}

	code = jsregex_backend_code(compiled);
	if (code == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (pcre2_pattern_info(code, PCRE2_INFO_NAMECOUNT, &name_count) != 0
			|| pcre2_pattern_info(code, PCRE2_INFO_NAMEENTRYSIZE,
				&name_entry_size) != 0
			|| pcre2_pattern_info(code, PCRE2_INFO_NAMETABLE,
				&name_table) != 0) {
		errno = EINVAL;
		return -1;
	}
	if (index >= name_count || name_entry_size < 2 || name_table == NULL) {
		errno = EINVAL;
		return -1;
	}

	entry = name_table + index * name_entry_size;
	while (name_len + 1 < name_entry_size && entry[name_len + 1] != 0) {
		name_len++;
	}
	if (name_len_ptr != NULL) {
		*name_len_ptr = name_len;
	}
	if (capture_index_ptr != NULL) {
		*capture_index_ptr = (uint32_t)entry[0];
	}
	if (name_buf != NULL) {
		if (name_cap < name_len) {
			errno = ENOBUFS;
			return -1;
		}
		if (name_len > 0) {
			memcpy(name_buf, entry + 1, name_len * sizeof(*name_buf));
		}
	}
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
jsregex_exec_u_literal_sequence_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		size_t start_index, jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || pattern == NULL || pattern_len == 0
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

	index = start_index;
	while (index < subject_len) {
		size_t end_index = index + pattern_len;

		if (!jsregex_is_code_point_boundary(subject, subject_len, index)) {
			if (jsregex_advance_string_index_utf16(subject, subject_len, index,
					1, &index) < 0) {
				return -1;
			}
			continue;
		}
		if (end_index > subject_len) {
			break;
		}
		if (memcmp(subject + index, pattern,
				pattern_len * sizeof(*pattern)) == 0
				&& jsregex_is_code_point_boundary(subject, subject_len,
					end_index)) {
			result_ptr->matched = 1;
			result_ptr->start = index;
			result_ptr->end = end_index;
			result_ptr->slot_count = 1;
			return 0;
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
jsregex_exec_u_literal_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || members == NULL || members_len == 0
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

	index = start_index;
	while (index < subject_len) {
		int matched;
		size_t end_index;

		if (jsregex_u_literal_class_match_at(subject, subject_len,
				members, members_len, index, 0, &matched,
				&end_index) < 0) {
			return -1;
		}
		if (matched) {
			result_ptr->matched = 1;
			result_ptr->start = index;
			result_ptr->end = end_index;
			result_ptr->slot_count = 1;
			return 0;
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
jsregex_exec_u_literal_negated_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || members == NULL || members_len == 0
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

	index = start_index;
	while (index < subject_len) {
		int matched;
		size_t end_index;

		if (jsregex_u_literal_class_match_at(subject, subject_len,
				members, members_len, index, 1, &matched,
				&end_index) < 0) {
			return -1;
		}
		if (matched) {
			result_ptr->matched = 1;
			result_ptr->start = index;
			result_ptr->end = end_index;
			result_ptr->slot_count = 1;
			return 0;
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
jsregex_exec_u_literal_range_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t start_index, jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || ranges == NULL || range_count == 0
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

	index = start_index;
	while (index < subject_len) {
		int matched;
		size_t end_index;

		if (jsregex_u_literal_range_class_match_at(subject, subject_len,
				ranges, range_count, index, 0, &matched, &end_index) < 0) {
			return -1;
		}
		if (matched) {
			result_ptr->matched = 1;
			result_ptr->start = index;
			result_ptr->end = end_index;
			result_ptr->slot_count = 1;
			return 0;
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
jsregex_exec_u_literal_negated_range_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t start_index, jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || ranges == NULL || range_count == 0
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

	index = start_index;
	while (index < subject_len) {
		int matched;
		size_t end_index;

		if (jsregex_u_literal_range_class_match_at(subject, subject_len,
				ranges, range_count, index, 1, &matched, &end_index) < 0) {
			return -1;
		}
		if (matched) {
			result_ptr->matched = 1;
			result_ptr->start = index;
			result_ptr->end = end_index;
			result_ptr->slot_count = 1;
			return 0;
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
jsregex_exec_u_predefined_class_utf16(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		size_t start_index, jsregex_exec_result_t *result_ptr)
{
	size_t index;

	if (result_ptr == NULL || (subject_len > 0 && subject == NULL)) {
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
		int matched;
		size_t end_index;

		if (jsregex_u_predefined_class_match_at(subject, subject_len, kind,
				index, &matched, &end_index) < 0) {
			return -1;
		}
		if (matched) {
			result_ptr->matched = 1;
			result_ptr->start = index;
			result_ptr->end = end_index;
			result_ptr->slot_count = 1;
			return 0;
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
jsregex_search_u_literal_sequence_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		size_t start_index, jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_sequence_utf16(subject, subject_len,
			pattern, pattern_len, start_index, &result) < 0) {
		return -1;
	}
	result_ptr->matched = result.matched;
	result_ptr->start = result.start;
	result_ptr->end = result.end;
	return 0;
}

int
jsregex_search_u_literal_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_class_utf16(subject, subject_len,
			members, members_len, start_index, &result) < 0) {
		return -1;
	}
	result_ptr->matched = result.matched;
	result_ptr->start = result.start;
	result_ptr->end = result.end;
	return 0;
}

int
jsregex_search_u_literal_negated_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_negated_class_utf16(subject, subject_len,
			members, members_len, start_index, &result) < 0) {
		return -1;
	}
	result_ptr->matched = result.matched;
	result_ptr->start = result.start;
	result_ptr->end = result.end;
	return 0;
}

int
jsregex_search_u_literal_range_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t start_index, jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_range_class_utf16(subject, subject_len,
			ranges, range_count, start_index, &result) < 0) {
		return -1;
	}
	result_ptr->matched = result.matched;
	result_ptr->start = result.start;
	result_ptr->end = result.end;
	return 0;
}

int
jsregex_search_u_literal_negated_range_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *ranges, size_t range_count,
		size_t start_index, jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_literal_negated_range_class_utf16(subject, subject_len,
			ranges, range_count, start_index, &result) < 0) {
		return -1;
	}
	result_ptr->matched = result.matched;
	result_ptr->start = result.start;
	result_ptr->end = result.end;
	return 0;
}

int
jsregex_search_u_predefined_class_utf16(const uint16_t *subject,
		size_t subject_len, jsregex_u_predefined_class_kind_t kind,
		size_t start_index, jsregex_search_result_t *result_ptr)
{
	jsregex_exec_result_t result;

	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (jsregex_exec_u_predefined_class_utf16(subject, subject_len, kind,
			start_index, &result) < 0) {
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

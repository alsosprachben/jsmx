#ifndef JSREGEX_H
#define JSREGEX_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"

#if JSMX_WITH_REGEX

typedef enum jsregex_flag_e {
	JSREGEX_FLAG_GLOBAL = 1u << 0,
	JSREGEX_FLAG_IGNORE_CASE = 1u << 1,
	JSREGEX_FLAG_MULTILINE = 1u << 2,
	JSREGEX_FLAG_DOT_ALL = 1u << 3,
	JSREGEX_FLAG_UNICODE = 1u << 4,
	JSREGEX_FLAG_STICKY = 1u << 5
} jsregex_flag_t;

typedef struct jsregex_compiled_s {
	uintptr_t backend_code;
	uint32_t flags;
	uint32_t capture_count;
} jsregex_compiled_t;

typedef struct jsregex_exec_result_s {
	int matched;
	size_t start;
	size_t end;
	size_t slot_count;
} jsregex_exec_result_t;

typedef struct jsregex_search_result_s {
	int matched;
	size_t start;
	size_t end;
} jsregex_search_result_t;

int jsregex_compile_utf16(const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_compiled_t *compiled_ptr);
void jsregex_release(jsregex_compiled_t *compiled_ptr);
int jsregex_exec_utf16(const jsregex_compiled_t *compiled,
		const uint16_t *subject, size_t subject_len, size_t start_index,
		size_t *offsets, size_t offsets_cap,
		jsregex_exec_result_t *result_ptr);
int jsregex_exec_u_literal_surrogate_utf16(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t start_index,
		jsregex_exec_result_t *result_ptr);
int jsregex_exec_u_literal_sequence_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		size_t start_index, jsregex_exec_result_t *result_ptr);
int jsregex_exec_u_literal_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_exec_result_t *result_ptr);
int jsregex_exec_u_literal_negated_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_exec_result_t *result_ptr);
int jsregex_test_u_literal_surrogate_utf16(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t start_index,
		int *matched_ptr);
int jsregex_search_u_literal_surrogate_utf16(const uint16_t *subject,
		size_t subject_len, uint16_t surrogate_unit, size_t start_index,
		jsregex_search_result_t *result_ptr);
int jsregex_search_u_literal_sequence_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *pattern, size_t pattern_len,
		size_t start_index, jsregex_search_result_t *result_ptr);
int jsregex_search_u_literal_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_search_result_t *result_ptr);
int jsregex_search_u_literal_negated_class_utf16(const uint16_t *subject,
		size_t subject_len, const uint16_t *members, size_t members_len,
		size_t start_index, jsregex_search_result_t *result_ptr);
int jsregex_search_utf16(const uint16_t *subject, size_t subject_len,
		const uint16_t *pattern, size_t pattern_len,
		const uint16_t *flags, size_t flags_len,
		jsregex_search_result_t *result_ptr);
int jsregex_advance_string_index_utf16(const uint16_t *subject,
		size_t subject_len, size_t index, int unicode,
		size_t *next_index_ptr);

#endif

#endif

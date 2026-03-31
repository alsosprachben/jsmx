#ifndef JSMETHOD_H
#define JSMETHOD_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"
#include "jsstr.h"

typedef enum jsmethod_error_kind_e {
	JSMETHOD_ERROR_NONE = 0,
	JSMETHOD_ERROR_TYPE = 1,
	JSMETHOD_ERROR_RANGE = 2,
	JSMETHOD_ERROR_ABRUPT = 3,
	JSMETHOD_ERROR_SYNTAX = 4
} jsmethod_error_kind_t;

typedef struct jsmethod_error_s {
	jsmethod_error_kind_t kind;
	const char *message;
} jsmethod_error_t;

typedef int (*jsmethod_to_string_fn)(void *ctx, jsstr16_t *out,
		jsmethod_error_t *error);

typedef enum jsmethod_value_kind_e {
	JSMETHOD_VALUE_UNDEFINED = 0,
	JSMETHOD_VALUE_NULL = 1,
	JSMETHOD_VALUE_BOOL = 2,
	JSMETHOD_VALUE_NUMBER = 3,
	JSMETHOD_VALUE_STRING_UTF8 = 4,
	JSMETHOD_VALUE_STRING_UTF16 = 5,
	JSMETHOD_VALUE_SYMBOL = 6,
	JSMETHOD_VALUE_COERCIBLE = 7
} jsmethod_value_kind_t;

typedef struct jsmethod_value_s {
	uint8_t kind;
	uint8_t reserved[7];
	union {
		int boolean;
		double number;
		struct {
			const uint8_t *bytes;
			size_t len;
		} utf8;
		struct {
			const uint16_t *codeunits;
			size_t len;
		} utf16;
		struct {
			void *ctx;
			jsmethod_to_string_fn fn;
		} coercible;
	} as;
} jsmethod_value_t;

typedef struct jsmethod_string_normalize_sizes_s {
	size_t this_storage_len;
	size_t form_storage_len;
	size_t workspace_len;
	size_t result_len;
} jsmethod_string_normalize_sizes_t;

typedef struct jsmethod_string_repeat_sizes_s {
	size_t result_len;
} jsmethod_string_repeat_sizes_t;

typedef struct jsmethod_string_pad_sizes_s {
	size_t result_len;
} jsmethod_string_pad_sizes_t;

typedef struct jsmethod_string_concat_sizes_s {
	size_t result_len;
} jsmethod_string_concat_sizes_t;

void jsmethod_error_clear(jsmethod_error_t *error);

jsmethod_value_t jsmethod_value_undefined(void);
jsmethod_value_t jsmethod_value_null(void);
jsmethod_value_t jsmethod_value_bool(int boolean);
jsmethod_value_t jsmethod_value_number(double number);
jsmethod_value_t jsmethod_value_string_utf8(const uint8_t *bytes, size_t len);
jsmethod_value_t jsmethod_value_string_utf16(const uint16_t *codeunits, size_t len);
jsmethod_value_t jsmethod_value_symbol(void);
jsmethod_value_t jsmethod_value_coercible(void *ctx, jsmethod_to_string_fn fn);

int jsmethod_value_to_string(jsstr16_t *out, jsmethod_value_t value,
		int require_object_coercible, jsmethod_error_t *error);

int jsmethod_string_to_lower_case(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_to_upper_case(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_to_locale_lower_case(jsstr16_t *out,
		jsmethod_value_t this_value, int have_locale,
		jsmethod_value_t locale_value, uint16_t *locale_storage,
		size_t locale_storage_cap, jsmethod_error_t *error);
int jsmethod_string_to_locale_upper_case(jsstr16_t *out,
		jsmethod_value_t this_value, int have_locale,
		jsmethod_value_t locale_value, uint16_t *locale_storage,
		size_t locale_storage_cap, jsmethod_error_t *error);
int jsmethod_string_to_well_formed(jsstr16_t *out,
		jsmethod_value_t this_value, jsmethod_error_t *error);
int jsmethod_string_is_well_formed(int *is_well_formed,
		jsmethod_value_t this_value, uint16_t *storage, size_t storage_cap,
		jsmethod_error_t *error);
int jsmethod_string_trim(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_trim_start(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_trim_end(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_trim_left(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_trim_right(jsstr16_t *out, jsmethod_value_t this_value,
		jsmethod_error_t *error);
int jsmethod_string_concat_measure(jsmethod_value_t this_value,
		size_t arg_count, const jsmethod_value_t *args,
		jsmethod_string_concat_sizes_t *sizes,
		jsmethod_error_t *error);
int jsmethod_string_concat(jsstr16_t *out, jsmethod_value_t this_value,
		size_t arg_count, const jsmethod_value_t *args,
		jsmethod_error_t *error);
int jsmethod_string_repeat_measure(jsmethod_value_t this_value,
		int have_count, jsmethod_value_t count_value,
		jsmethod_string_repeat_sizes_t *sizes,
		jsmethod_error_t *error);
int jsmethod_string_repeat(jsstr16_t *out, jsmethod_value_t this_value,
		int have_count, jsmethod_value_t count_value,
		jsmethod_error_t *error);
int jsmethod_string_pad_start_measure(jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes,
		jsmethod_error_t *error);
int jsmethod_string_pad_start(jsstr16_t *out, jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		jsmethod_error_t *error);
int jsmethod_string_pad_end_measure(jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes,
		jsmethod_error_t *error);
int jsmethod_string_pad_end(jsstr16_t *out, jsmethod_value_t this_value,
		int have_max_length, jsmethod_value_t max_length_value,
		int have_fill_string, jsmethod_value_t fill_string_value,
		jsmethod_error_t *error);
int jsmethod_string_char_at(jsstr16_t *out, jsmethod_value_t this_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error);
int jsmethod_string_at(jsstr16_t *out, int *has_value_ptr,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);
int jsmethod_string_char_code_at(double *value_ptr,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);
int jsmethod_string_code_point_at(int *has_value_ptr, double *value_ptr,
		jsmethod_value_t this_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);
int jsmethod_string_slice(jsstr16_t *out, jsmethod_value_t this_value,
		int have_start, jsmethod_value_t start_value,
		int have_end, jsmethod_value_t end_value,
		jsmethod_error_t *error);
int jsmethod_string_substring(jsstr16_t *out, jsmethod_value_t this_value,
		int have_start, jsmethod_value_t start_value,
		int have_end, jsmethod_value_t end_value,
		jsmethod_error_t *error);
int jsmethod_string_substr(jsstr16_t *out, jsmethod_value_t this_value,
		int have_start, jsmethod_value_t start_value,
		int have_length, jsmethod_value_t length_value,
		jsmethod_error_t *error);
int jsmethod_string_index_of(ssize_t *index_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);
int jsmethod_string_last_index_of(ssize_t *index_ptr,
		jsmethod_value_t this_value, jsmethod_value_t search_value,
		int have_position, jsmethod_value_t position_value,
		jsmethod_error_t *error);
int jsmethod_string_includes(int *result_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);
int jsmethod_string_starts_with(int *result_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_position,
		jsmethod_value_t position_value, jsmethod_error_t *error);
int jsmethod_string_ends_with(int *result_ptr, jsmethod_value_t this_value,
		jsmethod_value_t search_value, int have_end_position,
		jsmethod_value_t end_position_value, jsmethod_error_t *error);
#if JSMX_WITH_REGEX
int jsmethod_string_search_regex(ssize_t *index_ptr,
		jsmethod_value_t this_value, jsmethod_value_t pattern_value,
		int have_flags, jsmethod_value_t flags_value,
		jsmethod_error_t *error);
#endif
int jsmethod_string_normalize_measure(jsmethod_value_t this_value,
		int have_form, jsmethod_value_t form_value,
		jsmethod_string_normalize_sizes_t *sizes,
		jsmethod_error_t *error);
int jsmethod_string_normalize_into(jsstr16_t *out, jsmethod_value_t this_value,
		uint16_t *this_storage, size_t this_storage_cap,
		int have_form, jsmethod_value_t form_value,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsmethod_error_t *error);
int jsmethod_string_normalize(jsstr16_t *out, jsmethod_value_t this_value,
		int have_form, jsmethod_value_t form_value,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsmethod_error_t *error);

#endif

#ifndef JSVAL_H
#define JSVAL_H

#include <stddef.h>
#include <stdint.h>

#include "jsmethod.h"
#include "jsmn.h"

typedef uint32_t jsval_off_t;

#define JSVAL_PAGES_MAGIC 0x4a535650u
#define JSVAL_PAGES_VERSION 1u

typedef enum jsval_repr_e {
	JSVAL_REPR_INLINE = 0,
	JSVAL_REPR_NATIVE = 1,
	JSVAL_REPR_JSON = 2
} jsval_repr_t;

typedef enum jsval_kind_e {
	JSVAL_KIND_UNDEFINED = 0,
	JSVAL_KIND_NULL = 1,
	JSVAL_KIND_BOOL = 2,
	JSVAL_KIND_NUMBER = 3,
	JSVAL_KIND_STRING = 4,
	JSVAL_KIND_OBJECT = 5,
	JSVAL_KIND_ARRAY = 6
} jsval_kind_t;

typedef struct jsval_s {
	uint8_t kind;
	uint8_t repr;
	uint16_t reserved;
	jsval_off_t off;
	union {
		int boolean;
		double number;
		uint32_t index;
	} as;
} jsval_t;

typedef struct jsval_pages_s {
	uint32_t magic;
	uint16_t version;
	uint16_t header_size;
	uint32_t total_len;
	uint32_t used_len;
	jsval_t root;
} jsval_pages_t;

typedef struct jsval_region_s {
	uint8_t *base;
	size_t len;
	size_t used;
	jsval_pages_t *pages;
} jsval_region_t;

void jsval_region_init(jsval_region_t *region, void *buf, size_t len);
void jsval_region_rebase(jsval_region_t *region, void *buf, size_t len);
size_t jsval_region_remaining(jsval_region_t *region);
size_t jsval_pages_head_size(void);
const jsval_pages_t *jsval_region_pages(const jsval_region_t *region);
int jsval_region_root(jsval_region_t *region, jsval_t *value_ptr);
int jsval_region_set_root(jsval_region_t *region, jsval_t value);

int jsval_is_native(jsval_t value);
int jsval_is_json_backed(jsval_t value);

jsval_t jsval_undefined(void);
jsval_t jsval_null(void);
jsval_t jsval_bool(int boolean);
jsval_t jsval_number(double number);

int jsval_string_new_utf8(jsval_region_t *region, const uint8_t *str, size_t len, jsval_t *value_ptr);
int jsval_string_new_utf16(jsval_region_t *region, const uint16_t *str, size_t len, jsval_t *value_ptr);
int jsval_object_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);
int jsval_array_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);

int jsval_json_parse(jsval_region_t *region, const uint8_t *json, size_t len, unsigned int token_cap, jsval_t *value_ptr);
int jsval_promote(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_promote_in_place(jsval_region_t *region, jsval_t *value_ptr);
int jsval_region_promote_root(jsval_region_t *region, jsval_t *value_ptr);
int jsval_promote_object_shallow_measure(jsval_region_t *region, jsval_t object, size_t prop_cap, size_t *bytes_ptr);
int jsval_promote_object_shallow(jsval_region_t *region, jsval_t object, size_t prop_cap, jsval_t *value_ptr);
int jsval_promote_object_shallow_in_place(jsval_region_t *region, jsval_t *value_ptr, size_t prop_cap);
int jsval_promote_array_shallow_measure(jsval_region_t *region, jsval_t array, size_t elem_cap, size_t *bytes_ptr);
int jsval_promote_array_shallow(jsval_region_t *region, jsval_t array, size_t elem_cap, jsval_t *value_ptr);
int jsval_promote_array_shallow_in_place(jsval_region_t *region, jsval_t *value_ptr, size_t elem_cap);
int jsval_copy_json(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr);

int jsval_string_copy_utf8(jsval_region_t *region, jsval_t value, uint8_t *buf, size_t cap, size_t *len_ptr);
size_t jsval_object_size(jsval_region_t *region, jsval_t object);
size_t jsval_array_length(jsval_region_t *region, jsval_t array);

int jsval_object_has_own_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, int *has_ptr);
int jsval_object_get_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t *value_ptr);
int jsval_object_set_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, jsval_t value);
int jsval_object_delete_utf8(jsval_region_t *region, jsval_t object, const uint8_t *key, size_t key_len, int *deleted_ptr);
int jsval_array_get(jsval_region_t *region, jsval_t array, size_t index, jsval_t *value_ptr);
int jsval_array_set(jsval_region_t *region, jsval_t array, size_t index, jsval_t value);
int jsval_array_push(jsval_region_t *region, jsval_t array, jsval_t value);
int jsval_array_set_length(jsval_region_t *region, jsval_t array, size_t new_len);

int jsval_to_number(jsval_region_t *region, jsval_t value, double *number_ptr);
int jsval_to_int32(jsval_region_t *region, jsval_t value, int32_t *result_ptr);
int jsval_to_uint32(jsval_region_t *region, jsval_t value,
		uint32_t *result_ptr);
int jsval_truthy(jsval_region_t *region, jsval_t value);
int jsval_strict_eq(jsval_region_t *region, jsval_t left, jsval_t right);
int jsval_abstract_eq(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr);
int jsval_abstract_ne(jsval_region_t *region, jsval_t left, jsval_t right,
		int *result_ptr);
int jsval_less_than(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_less_equal(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_greater_than(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_greater_equal(jsval_region_t *region, jsval_t left, jsval_t right, int *result_ptr);
int jsval_unary_plus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_unary_minus(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
int jsval_bitwise_not(jsval_region_t *region, jsval_t value,
		jsval_t *value_ptr);
int jsval_add(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_bitwise_and(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_bitwise_or(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_bitwise_xor(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_shift_left(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_shift_right(jsval_region_t *region, jsval_t left, jsval_t right,
		jsval_t *value_ptr);
int jsval_shift_right_unsigned(jsval_region_t *region, jsval_t left,
		jsval_t right, jsval_t *value_ptr);
int jsval_subtract(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_multiply(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_divide(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_remainder(jsval_region_t *region, jsval_t left, jsval_t right, jsval_t *value_ptr);
int jsval_method_string_to_lower_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_upper_case(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_locale_lower_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_locale_upper_case(jsval_region_t *region,
		jsval_t this_value, int have_locale, jsval_t locale_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_to_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_is_well_formed(jsval_region_t *region,
		jsval_t this_value, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_char_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_at(jsval_region_t *region, jsval_t this_value,
		int have_position, jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_char_code_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_code_point_at(jsval_region_t *region,
		jsval_t this_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_index_of(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_last_index_of(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_includes(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_position, jsval_t position_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_starts_with(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, int have_position,
		jsval_t position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_ends_with(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, int have_end_position,
		jsval_t end_position_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_normalize_measure(jsval_region_t *region,
		jsval_t this_value, int have_form, jsval_t form_value,
		jsmethod_string_normalize_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_normalize(jsval_region_t *region, jsval_t this_value,
		int have_form, jsval_t form_value,
		uint16_t *this_storage, size_t this_storage_cap,
		uint16_t *form_storage, size_t form_storage_cap,
		uint32_t *workspace, size_t workspace_cap,
		jsval_t *value_ptr, jsmethod_error_t *error);

#endif

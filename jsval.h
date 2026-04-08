#ifndef JSVAL_H
#define JSVAL_H

#include <stddef.h>
#include <stdint.h>

#include "jsmx_config.h"
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
	JSVAL_KIND_ARRAY = 6,
	JSVAL_KIND_REGEXP = 7,
	JSVAL_KIND_MATCH_ITERATOR = 8,
	JSVAL_KIND_URL = 9,
	JSVAL_KIND_URL_SEARCH_PARAMS = 10,
	JSVAL_KIND_SET = 11,
	JSVAL_KIND_MAP = 12,
	JSVAL_KIND_ITERATOR = 13
} jsval_kind_t;

typedef enum jsval_iterator_selector_e {
	JSVAL_ITERATOR_SELECTOR_DEFAULT = 0,
	JSVAL_ITERATOR_SELECTOR_KEYS = 1,
	JSVAL_ITERATOR_SELECTOR_VALUES = 2,
	JSVAL_ITERATOR_SELECTOR_ENTRIES = 3
} jsval_iterator_selector_t;

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

#if JSMX_WITH_REGEX
typedef struct jsval_regexp_info_s {
	uint32_t flags;
	uint32_t capture_count;
	size_t last_index;
} jsval_regexp_info_t;
#endif

typedef struct jsval_replace_call_s {
	jsval_t match;
	size_t capture_count;
	const jsval_t *captures;
	size_t offset;
	jsval_t input;
	jsval_t groups;
} jsval_replace_call_t;

typedef int (*jsval_replace_callback_fn)(jsval_region_t *region, void *ctx,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error);

void jsval_region_init(jsval_region_t *region, void *buf, size_t len);
void jsval_region_rebase(jsval_region_t *region, void *buf, size_t len);
size_t jsval_region_remaining(jsval_region_t *region);
int jsval_region_alloc(jsval_region_t *region, size_t len, size_t align,
		void **ptr_ptr);
int jsval_region_measure_alloc(const jsval_region_t *region, size_t *used_ptr,
		size_t len, size_t align);
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
int jsval_object_key_at(jsval_region_t *region, jsval_t object, size_t index, jsval_t *key_ptr);
int jsval_object_value_at(jsval_region_t *region, jsval_t object, size_t index, jsval_t *value_ptr);
int jsval_object_copy_own(jsval_region_t *region, jsval_t dst, jsval_t src);
int jsval_object_clone_own(jsval_region_t *region, jsval_t src, size_t capacity, jsval_t *value_ptr);
int jsval_set_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);
int jsval_set_clone(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr);
int jsval_set_size(jsval_region_t *region, jsval_t set, size_t *size_ptr);
int jsval_set_has(jsval_region_t *region, jsval_t set, jsval_t key,
		int *has_ptr);
int jsval_set_add(jsval_region_t *region, jsval_t set, jsval_t key);
int jsval_set_delete(jsval_region_t *region, jsval_t set, jsval_t key,
		int *deleted_ptr);
int jsval_set_clear(jsval_region_t *region, jsval_t set);
int jsval_map_new(jsval_region_t *region, size_t cap, jsval_t *value_ptr);
int jsval_map_clone(jsval_region_t *region, jsval_t src, size_t capacity,
		jsval_t *value_ptr);
int jsval_map_size(jsval_region_t *region, jsval_t map, size_t *size_ptr);
int jsval_map_has(jsval_region_t *region, jsval_t map, jsval_t key,
		int *has_ptr);
int jsval_map_get(jsval_region_t *region, jsval_t map, jsval_t key,
		jsval_t *value_ptr);
int jsval_map_set(jsval_region_t *region, jsval_t map, jsval_t key,
		jsval_t value);
int jsval_map_delete(jsval_region_t *region, jsval_t map, jsval_t key,
		int *deleted_ptr);
int jsval_map_clear(jsval_region_t *region, jsval_t map);
int jsval_map_key_at(jsval_region_t *region, jsval_t map, size_t index,
		jsval_t *key_ptr);
int jsval_map_value_at(jsval_region_t *region, jsval_t map, size_t index,
		jsval_t *value_ptr);
int jsval_get_iterator(jsval_region_t *region, jsval_t iterable,
		jsval_iterator_selector_t selector, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_iterator_next(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_iterator_next_entry(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *key_ptr, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_array_get(jsval_region_t *region, jsval_t array, size_t index, jsval_t *value_ptr);
int jsval_array_set(jsval_region_t *region, jsval_t array, size_t index, jsval_t value);
int jsval_array_clone_dense(jsval_region_t *region, jsval_t src, size_t capacity, jsval_t *value_ptr);
int jsval_array_splice_dense(jsval_region_t *region, jsval_t array, size_t start, size_t delete_count, const jsval_t *insert_values, size_t insert_count, jsval_t *removed_ptr);
int jsval_array_push(jsval_region_t *region, jsval_t array, jsval_t value);
int jsval_array_pop(jsval_region_t *region, jsval_t array, jsval_t *value_ptr);
int jsval_array_shift(jsval_region_t *region, jsval_t array, jsval_t *value_ptr);
int jsval_array_unshift(jsval_region_t *region, jsval_t array, jsval_t value);
int jsval_array_set_length(jsval_region_t *region, jsval_t array, size_t new_len);

int jsval_url_new(jsval_region_t *region, jsval_t input_value, int have_base,
		jsval_t base_value, jsval_t *value_ptr);
int jsval_url_href(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_origin(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_origin_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_protocol(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_username(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_password(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_host(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_host_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_hostname(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_hostname_display(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_port(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_pathname(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_search(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_hash(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_search_params(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_set_href(jsval_region_t *region, jsval_t url_value,
		jsval_t href_value);
int jsval_url_set_protocol(jsval_region_t *region, jsval_t url_value,
		jsval_t protocol_value);
int jsval_url_set_username(jsval_region_t *region, jsval_t url_value,
		jsval_t username_value);
int jsval_url_set_password(jsval_region_t *region, jsval_t url_value,
		jsval_t password_value);
int jsval_url_set_host(jsval_region_t *region, jsval_t url_value,
		jsval_t host_value);
int jsval_url_set_hostname(jsval_region_t *region, jsval_t url_value,
		jsval_t hostname_value);
int jsval_url_set_port(jsval_region_t *region, jsval_t url_value,
		jsval_t port_value);
int jsval_url_set_pathname(jsval_region_t *region, jsval_t url_value,
		jsval_t pathname_value);
int jsval_url_set_search(jsval_region_t *region, jsval_t url_value,
		jsval_t search_value);
int jsval_url_set_hash(jsval_region_t *region, jsval_t url_value,
		jsval_t hash_value);
int jsval_url_to_string(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);
int jsval_url_to_json(jsval_region_t *region, jsval_t url_value,
		jsval_t *value_ptr);

int jsval_url_search_params_new(jsval_region_t *region, jsval_t init_value,
		jsval_t *value_ptr);
int jsval_url_search_params_size(jsval_region_t *region, jsval_t params_value,
		jsval_t *value_ptr);
int jsval_url_search_params_append(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t value_value);
int jsval_url_search_params_delete(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value);
int jsval_url_search_params_get(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t *value_ptr);
int jsval_url_search_params_get_all(jsval_region_t *region,
		jsval_t params_value, jsval_t name_value, jsval_t *value_ptr);
int jsval_url_search_params_has(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t *value_ptr);
int jsval_url_search_params_set(jsval_region_t *region, jsval_t params_value,
		jsval_t name_value, jsval_t value_value);
int jsval_url_search_params_sort(jsval_region_t *region, jsval_t params_value);
int jsval_url_search_params_to_string(jsval_region_t *region,
		jsval_t params_value, jsval_t *value_ptr);

int jsval_to_number(jsval_region_t *region, jsval_t value, double *number_ptr);
int jsval_to_int32(jsval_region_t *region, jsval_t value, int32_t *result_ptr);
int jsval_to_uint32(jsval_region_t *region, jsval_t value,
		uint32_t *result_ptr);
int jsval_is_nullish(jsval_t value);
int jsval_typeof(jsval_region_t *region, jsval_t value, jsval_t *value_ptr);
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
int jsval_method_string_trim(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_start(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_end(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_left(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_trim_right(jsval_region_t *region, jsval_t this_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_concat_measure(jsval_region_t *region,
		jsval_t this_value, size_t arg_count, const jsval_t *args,
		jsmethod_string_concat_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_concat(jsval_region_t *region, jsval_t this_value,
		size_t arg_count, const jsval_t *args, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_measure(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_replace(jsval_region_t *region, jsval_t this_value,
		jsval_t search_value, jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_measure(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsmethod_string_replace_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_replace_all(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value, jsval_t replacement_value,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_fn(jsval_region_t *region,
		jsval_t this_value, jsval_t search_value,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_repeat_measure(jsval_region_t *region,
		jsval_t this_value, int have_count, jsval_t count_value,
		jsmethod_string_repeat_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_repeat(jsval_region_t *region, jsval_t this_value,
		int have_count, jsval_t count_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_pad_start_measure(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_pad_start(jsval_region_t *region, jsval_t this_value,
		int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_pad_end_measure(jsval_region_t *region,
		jsval_t this_value, int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value,
		jsmethod_string_pad_sizes_t *sizes,
		jsmethod_error_t *error);
int jsval_method_string_pad_end(jsval_region_t *region, jsval_t this_value,
		int have_max_length, jsval_t max_length_value,
		int have_fill_string, jsval_t fill_string_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
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
int jsval_method_string_slice(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_substring(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_end, jsval_t end_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_substr(jsval_region_t *region, jsval_t this_value,
		int have_start, jsval_t start_value,
		int have_length, jsval_t length_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split(jsval_region_t *region, jsval_t this_value,
		int have_separator, jsval_t separator_value,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
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
#if JSMX_WITH_REGEX
int jsval_regexp_new(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_regexp_new_jit(jsval_region_t *region, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_regexp_source(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr);
int jsval_regexp_global(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_ignore_case(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_multiline(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_dot_all(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_unicode(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_sticky(jsval_region_t *region, jsval_t regexp_value,
		int *result_ptr);
int jsval_regexp_flags(jsval_region_t *region, jsval_t regexp_value,
		jsval_t *value_ptr);
int jsval_regexp_info(jsval_region_t *region, jsval_t regexp_value,
		jsval_regexp_info_t *info_ptr);
int jsval_regexp_get_last_index(jsval_region_t *region, jsval_t regexp_value,
		size_t *last_index_ptr);
int jsval_regexp_set_last_index(jsval_region_t *region, jsval_t regexp_value,
		size_t last_index);
int jsval_regexp_test(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, int *result_ptr, jsmethod_error_t *error);
int jsval_regexp_exec(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_regexp_match_all(jsval_region_t *region, jsval_t regexp_value,
		jsval_t input_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_match_iterator_next(jsval_region_t *region, jsval_t iterator_value,
		int *done_ptr, jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match(jsval_region_t *region, jsval_t this_value,
		int have_regexp, jsval_t regexp_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all(jsval_region_t *region, jsval_t this_value,
		int have_regexp, jsval_t regexp_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_surrogate(
		jsval_region_t *region, jsval_t this_value,
		uint16_t surrogate_unit, jsval_t *value_ptr,
		jsmethod_error_t *error);
#define JSVAL_DECLARE_U_PREDEFINED_CLASS_API(name) \
int jsval_method_string_search_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_match_all_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_match_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, int global, \
		jsval_t *value_ptr, jsmethod_error_t *error); \
int jsval_method_string_replace_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_t replacement_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_replace_all_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_t replacement_value, jsval_t *value_ptr, \
		jsmethod_error_t *error); \
int jsval_method_string_replace_u_##name##_class_fn( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_replace_callback_fn callback, void *callback_ctx, \
		jsval_t *value_ptr, jsmethod_error_t *error); \
int jsval_method_string_replace_all_u_##name##_class_fn( \
		jsval_region_t *region, jsval_t this_value, \
		jsval_replace_callback_fn callback, void *callback_ctx, \
		jsval_t *value_ptr, jsmethod_error_t *error); \
int jsval_method_string_split_u_##name##_class( \
		jsval_region_t *region, jsval_t this_value, int have_limit, \
		jsval_t limit_value, jsval_t *value_ptr, \
		jsmethod_error_t *error);
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(digit)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(negated_digit)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(whitespace)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(negated_whitespace)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(word)
JSVAL_DECLARE_U_PREDEFINED_CLASS_API(negated_word)
#undef JSVAL_DECLARE_U_PREDEFINED_CLASS_API
int jsval_method_string_search_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_all_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_match_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_match_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, int global,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_surrogate(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_surrogate_fn(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_surrogate_fn(
		jsval_region_t *region, jsval_t this_value, uint16_t surrogate_unit,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_sequence_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_sequence_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_t replacement_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_u_literal_negated_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_replace_all_u_literal_negated_range_class_fn(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count,
		jsval_replace_callback_fn callback, void *callback_ctx,
		jsval_t *value_ptr, jsmethod_error_t *error);
int jsval_method_string_split_u_literal_surrogate(jsval_region_t *region,
		jsval_t this_value, uint16_t surrogate_unit, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_sequence(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *pattern, size_t pattern_len,
		int have_limit, jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_negated_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *members, size_t members_len, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_split_u_literal_negated_range_class(
		jsval_region_t *region, jsval_t this_value,
		const uint16_t *ranges, size_t range_count, int have_limit,
		jsval_t limit_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
int jsval_method_string_search_regex(jsval_region_t *region,
		jsval_t this_value, jsval_t pattern_value,
		int have_flags, jsval_t flags_value, jsval_t *value_ptr,
		jsmethod_error_t *error);
#endif
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

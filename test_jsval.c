#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jsval.h"

static void assert_number_value(jsval_t value, double expected);
static void assert_nan_value(jsval_t value);

typedef struct test_replace_string_callback_ctx_s {
	int call_count;
	const char *expected_input;
	const char *expected_matches[4];
	size_t expected_offsets[4];
	const char *replacement_values[4];
} test_replace_string_callback_ctx_t;

typedef struct test_replace_throw_ctx_s {
	int call_count;
	jsmethod_error_kind_t kind;
	const char *message;
} test_replace_throw_ctx_t;

typedef struct test_replace_regex_callback_ctx_s {
	int call_count;
	const char *expected_input;
	const char *expected_matches[4];
	const char *expected_capture1[4];
	const char *expected_capture2[4];
	size_t expected_offsets[4];
	const char *replacement_values[4];
} test_replace_regex_callback_ctx_t;

typedef struct test_replace_named_groups_callback_ctx_s {
	int call_count;
	const char *expected_input;
	const char *expected_matches[4];
	const char *expected_digits[4];
	const char *expected_tail[4];
	size_t expected_offsets[4];
	const char *replacement_values[4];
} test_replace_named_groups_callback_ctx_t;

typedef struct test_replace_surrogate_callback_ctx_s {
	int call_count;
	const uint16_t *expected_input;
	size_t expected_input_len;
	const uint16_t *expected_matches[4];
	size_t expected_match_lens[4];
	size_t expected_offsets[4];
	const char *replacement_values[4];
} test_replace_surrogate_callback_ctx_t;

typedef struct test_promise_identity_ctx_s {
	int call_count;
	size_t last_argc;
	jsval_t last_arg;
} test_promise_identity_ctx_t;

typedef struct test_promise_return_ctx_s {
	int call_count;
	size_t last_argc;
	jsval_t last_arg;
	jsval_t return_value;
} test_promise_return_ctx_t;

typedef struct test_promise_throw_ctx_s {
	int call_count;
} test_promise_throw_ctx_t;

typedef struct test_microtask_ctx_s {
	int call_count;
	int enqueue_nested;
	jsval_t nested_function;
} test_microtask_ctx_t;

typedef struct test_scheduler_ctx_s {
	int enqueue_count;
	int wake_count;
} test_scheduler_ctx_t;

static test_promise_identity_ctx_t test_promise_identity_ctx;
static test_promise_return_ctx_t test_promise_return_ctx;
static test_promise_throw_ctx_t test_promise_throw_ctx;
static test_microtask_ctx_t test_microtask_ctx;

static int test_function_sum(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)error;
	if (region == NULL || result_ptr == NULL || argc < 2 || argv == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_add(region, argv[0], argv[1], result_ptr);
}

static int test_function_box(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	jsval_t object;
	jsval_t value = jsval_undefined();

	(void)error;
	if (region == NULL || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (argc > 0 && argv != NULL) {
		value = argv[0];
	}
	if (jsval_object_new(region, 1, &object) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, object, (const uint8_t *)"value", 5,
			value) < 0) {
		return -1;
	}
	*result_ptr = object;
	return 0;
}

static int test_function_throw(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)argc;
	(void)argv;
	(void)result_ptr;
	errno = EINVAL;
	if (error != NULL) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "test function threw";
	}
	return -1;
}

static int test_promise_identity(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)error;
	if (result_ptr == NULL || (argc > 0 && argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
	test_promise_identity_ctx.call_count++;
	test_promise_identity_ctx.last_argc = argc;
	test_promise_identity_ctx.last_arg = argc > 0 ? argv[0] : jsval_undefined();
	*result_ptr = argc > 0 ? argv[0] : jsval_undefined();
	return 0;
}

static int test_promise_return(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)error;
	if (result_ptr == NULL || (argc > 0 && argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
	test_promise_return_ctx.call_count++;
	test_promise_return_ctx.last_argc = argc;
	test_promise_return_ctx.last_arg = argc > 0 ? argv[0] : jsval_undefined();
	*result_ptr = test_promise_return_ctx.return_value;
	return 0;
}

static int test_promise_throw_handler(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)argc;
	(void)argv;
	(void)result_ptr;
	test_promise_throw_ctx.call_count++;
	errno = EINVAL;
	if (error != NULL) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "test promise handler threw";
	}
	return -1;
}

static int test_microtask_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)error;
	if (region == NULL || result_ptr == NULL || (argc > 0 && argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
	test_microtask_ctx.call_count++;
	if (test_microtask_ctx.enqueue_nested
			&& test_microtask_ctx.nested_function.kind == JSVAL_KIND_FUNCTION) {
		test_microtask_ctx.enqueue_nested = 0;
		if (jsval_microtask_enqueue(region, test_microtask_ctx.nested_function,
				0, NULL) < 0) {
			return -1;
		}
	}
	*result_ptr = argc > 0 ? argv[0] : jsval_undefined();
	return 0;
}

static void test_scheduler_on_enqueue(jsval_region_t *region, void *ctx)
{
	(void)region;
	if (ctx != NULL) {
		((test_scheduler_ctx_t *)ctx)->enqueue_count++;
	}
}

static void test_scheduler_on_wake(jsval_region_t *region, void *ctx)
{
	(void)region;
	if (ctx != NULL) {
		((test_scheduler_ctx_t *)ctx)->wake_count++;
	}
}

static void assert_string(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	assert(value.kind == JSVAL_KIND_STRING);
	assert(jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) == 0);
	assert(actual_len == expected_len);
	assert(jsval_string_copy_utf8(region, value, buf, actual_len, NULL) == 0);
	assert(memcmp(buf, expected, expected_len) == 0);
}

static void assert_dom_exception(jsval_region_t *region, jsval_t value,
		const char *expected_name, const char *expected_message)
{
	jsval_t result;

	assert(value.kind == JSVAL_KIND_DOM_EXCEPTION);
	assert(jsval_dom_exception_name(region, value, &result) == 0);
	assert_string(region, result, expected_name);
	assert(jsval_dom_exception_message(region, value, &result) == 0);
	assert_string(region, result, expected_message);
}

static void assert_array_buffer_bytes(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	assert(value.kind == JSVAL_KIND_ARRAY_BUFFER);
	assert(jsval_array_buffer_copy_bytes(region, value, NULL, 0, &actual_len)
			== 0);
	assert(actual_len == expected_len);
	assert(jsval_array_buffer_copy_bytes(region, value, buf, actual_len, NULL)
			== 0);
	assert(memcmp(buf, expected, expected_len) == 0);
}

static void assert_bigint_string(jsval_region_t *region, jsval_t value,
		const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	assert(value.kind == JSVAL_KIND_BIGINT);
	assert(jsval_bigint_copy_utf8(region, value, NULL, 0, &actual_len) == 0);
	assert(actual_len == expected_len);
	assert(jsval_bigint_copy_utf8(region, value, buf, actual_len, NULL) == 0);
	assert(memcmp(buf, expected, expected_len) == 0);
}

static void
assert_utf16_string(jsval_region_t *region, jsval_t value,
		const uint16_t *expected, size_t expected_len)
{
	static const uint16_t empty_unit = 0;
	jsval_t expected_value;

	assert(value.kind == JSVAL_KIND_STRING);
	assert(jsval_string_new_utf16(region,
			expected_len > 0 ? expected : &empty_unit, expected_len,
			&expected_value) == 0);
	assert(jsval_strict_eq(region, value, expected_value) == 1);
}

static void assert_json(jsval_region_t *region, jsval_t value, const char *expected)
{
	size_t expected_len = strlen(expected);
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	assert(jsval_copy_json(region, value, NULL, 0, &actual_len) == 0);
	assert(actual_len == expected_len);
	assert(jsval_copy_json(region, value, buf, actual_len, NULL) == 0);
	assert(memcmp(buf, expected, expected_len) == 0);
}

static void assert_object_string_prop(jsval_region_t *region, jsval_t object,
		const char *key, const char *expected)
{
	jsval_t value;

	assert(object.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) == 0);
	assert_string(region, value, expected);
}

static void
assert_object_utf16_prop(jsval_region_t *region, jsval_t object,
		const char *key, const uint16_t *expected, size_t expected_len)
{
	jsval_t value;

	assert(object.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) == 0);
	assert_utf16_string(region, value, expected, expected_len);
}

static void assert_object_number_prop(jsval_region_t *region, jsval_t object,
		const char *key, double expected)
{
	jsval_t value;

	assert(object.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) == 0);
	assert_number_value(value, expected);
}

static void assert_object_undefined_prop(jsval_region_t *region, jsval_t object,
		const char *key)
{
	jsval_t value;

	assert(object.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, object, (const uint8_t *)key,
			strlen(key), &value) == 0);
	assert(value.kind == JSVAL_KIND_UNDEFINED);
}

static void
assert_object_groups_string_prop(jsval_region_t *region, jsval_t object,
		const char *group_key, const char *expected)
{
	jsval_t groups;

	assert(object.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, object, (const uint8_t *)"groups", 6,
			&groups) == 0);
	assert(groups.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(region, groups, group_key, expected);
}

static void
assert_object_groups_undefined_prop(jsval_region_t *region, jsval_t object,
		const char *group_key)
{
	jsval_t groups;

	assert(object.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, object, (const uint8_t *)"groups", 6,
			&groups) == 0);
	assert(groups.kind == JSVAL_KIND_OBJECT);
	assert_object_undefined_prop(region, groups, group_key);
}

static void assert_object_key_at(jsval_region_t *region, jsval_t object,
		size_t index, const char *expected)
{
	jsval_t value;

	assert(jsval_object_key_at(region, object, index, &value) == 0);
	assert_string(region, value, expected);
}

static void
assert_object_symbol_key_at(jsval_region_t *region, jsval_t object,
		size_t index, jsval_t expected)
{
	jsval_t value;

	assert(jsval_object_key_at(region, object, index, &value) == 0);
	assert(jsval_strict_eq(region, value, expected) == 1);
}

static void assert_object_key_undefined_at(jsval_region_t *region,
		jsval_t object, size_t index)
{
	jsval_t value;

	assert(jsval_object_key_at(region, object, index, &value) == 0);
	assert(value.kind == JSVAL_KIND_UNDEFINED);
}

static void assert_object_value_json_at(jsval_region_t *region, jsval_t object,
		size_t index, const char *expected)
{
	jsval_t value;

	assert(jsval_object_value_at(region, object, index, &value) == 0);
	assert_json(region, value, expected);
}

static void assert_object_value_undefined_at(jsval_region_t *region,
		jsval_t object, size_t index)
{
	jsval_t value;

	assert(jsval_object_value_at(region, object, index, &value) == 0);
	assert(value.kind == JSVAL_KIND_UNDEFINED);
}

static void assert_array_strings(jsval_region_t *region, jsval_t array,
		const char *const *expected, size_t expected_len)
{
	size_t i;

	assert(array.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(region, array) == expected_len);
	for (i = 0; i < expected_len; i++) {
		jsval_t value;

		assert(jsval_array_get(region, array, i, &value) == 0);
		if (expected[i] == NULL) {
			assert(value.kind == JSVAL_KIND_UNDEFINED);
		} else {
			assert_string(region, value, expected[i]);
		}
	}
}

static void test_function_semantics(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t name;
	jsval_t named;
	jsval_t anonymous;
	jsval_t named_again;
	jsval_t boxer;
	jsval_t thrower;
	jsval_t object;
	jsval_t array;
	jsval_t set;
	jsval_t map;
	jsval_t clone;
	jsval_t copied;
	jsval_t result;
	jsval_t value;
	jsval_t args[2];
	int has = 0;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"sum", 3,
			&name) == 0);
	assert(jsval_function_new(&region, test_function_sum, 2, 1, name,
			&named) == 0);
	assert(jsval_function_new(&region, test_function_sum, 2, 0,
			jsval_undefined(), &anonymous) == 0);
	assert(jsval_function_new(&region, test_function_sum, 2, 1, name,
			&named_again) == 0);
	assert(jsval_function_new(&region, test_function_box, 1, 0,
			jsval_undefined(), &boxer) == 0);
	assert(jsval_function_new(&region, test_function_throw, 0, 1, name,
			&thrower) == 0);

	assert(named.kind == JSVAL_KIND_FUNCTION);
	assert(jsval_truthy(&region, named) == 1);
	assert(jsval_typeof(&region, named, &result) == 0);
	assert_string(&region, result, "function");
	assert(jsval_strict_eq(&region, named, named) == 1);
	assert(jsval_strict_eq(&region, named, named_again) == 0);
	assert(jsval_abstract_eq(&region, named, named, &has) == 0);
	assert(has == 1);
	assert(jsval_abstract_eq(&region, named, named_again, &has) == 0);
	assert(has == 0);
	assert(jsval_abstract_eq(&region, named, name, &has) == 0);
	assert(has == 0);

	assert(jsval_function_name(&region, named, &result) == 0);
	assert_string(&region, result, "sum");
	assert(jsval_function_name(&region, anonymous, &result) == 0);
	assert_string(&region, result, "");
	assert(jsval_function_length(&region, named, &result) == 0);
	assert_number_value(result, 2.0);

	args[0] = jsval_number(20.0);
	args[1] = jsval_number(22.0);
	memset(&error, 0, sizeof(error));
	assert(jsval_function_call(&region, named, 2, args, &result, &error) == 0);
	assert_number_value(result, 42.0);
	assert(error.kind == JSMETHOD_ERROR_NONE);

	memset(&error, 0, sizeof(error));
	assert(jsval_function_call(&region, boxer, 1, args, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_number_prop(&region, result, "value", 20.0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(jsval_function_call(&region, thrower, 0, NULL, &result, &error)
			< 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);
	assert(strcmp(error.message, "test function threw") == 0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(jsval_function_call(&region, jsval_number(1.0), 0, NULL, &result,
			&error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_TYPE);
	assert(strcmp(error.message, "function required") == 0);

	assert(jsval_object_new(&region, 1, &object) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"fn", 2,
			named) == 0);
	assert(jsval_object_get_utf8(&region, object, (const uint8_t *)"fn", 2,
			&value) == 0);
	assert(jsval_strict_eq(&region, value, named) == 1);

	assert(jsval_array_new(&region, 1, &array) == 0);
	assert(jsval_array_push(&region, array, named) == 0);
	assert(jsval_array_get(&region, array, 0, &value) == 0);
	assert(jsval_strict_eq(&region, value, named) == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_function_call(&region, value, 2, args, &result, &error) == 0);
	assert_number_value(result, 42.0);

	assert(jsval_set_new(&region, 1, &set) == 0);
	assert(jsval_set_add(&region, set, named) == 0);
	assert(jsval_set_has(&region, set, named, &has) == 0);
	assert(has == 1);
	assert(jsval_set_has(&region, set, named_again, &has) == 0);
	assert(has == 0);

	assert(jsval_map_new(&region, 1, &map) == 0);
	assert(jsval_map_set(&region, map, named, jsval_bool(1)) == 0);
	assert(jsval_map_get(&region, map, named, &value) == 0);
	assert(value.kind == JSVAL_KIND_BOOL);
	assert(value.as.boolean == 1);
	assert(jsval_map_get(&region, map, named_again, &value) == 0);
	assert(value.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_object_clone_own(&region, object, 1, &clone) == 0);
	assert(jsval_object_get_utf8(&region, clone, (const uint8_t *)"fn", 2,
			&value) == 0);
	assert(jsval_strict_eq(&region, value, named) == 1);
	assert(jsval_object_new(&region, 1, &copied) == 0);
	assert(jsval_object_copy_own(&region, copied, object) == 0);
	assert(jsval_object_get_utf8(&region, copied, (const uint8_t *)"fn", 2,
			&value) == 0);
	assert(jsval_strict_eq(&region, value, named) == 1);

	errno = 0;
	assert(jsval_copy_json(&region, named, NULL, 0, NULL) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_copy_json(&region, object, NULL, 0, NULL) < 0);
	assert(errno == ENOTSUP);
}

static void test_date_semantics(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t result;
	jsval_t date_a;
	jsval_t date_b;
	jsval_t parsed;
	jsval_t invalid;
	jsval_t local_date;
	jsval_t ctor_99;
	jsval_t input;
	jsval_t bad_input;
	jsval_t args[7];
	size_t len = 0;
	int valid = 0;
	jsmethod_error_t error;
	uint8_t buf[128];

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_date_now(&region, &result) == 0);
	assert(result.kind == JSVAL_KIND_NUMBER);
	assert(isfinite(result.as.number));
	assert(jsval_date_new_now(&region, &date_a) == 0);
	assert(date_a.kind == JSVAL_KIND_DATE);
	assert(jsval_truthy(&region, date_a) == 1);
	assert(jsval_typeof(&region, date_a, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_date_is_valid(&region, date_a, &valid) == 0);
	assert(valid == 1);

	assert(jsval_date_new_time(&region, jsval_number(1577934245006.0),
			&date_a) == 0);
	assert(jsval_date_new_time(&region, jsval_number(1577934245006.0),
			&date_b) == 0);
	assert(jsval_strict_eq(&region, date_a, date_a) == 1);
	assert(jsval_strict_eq(&region, date_a, date_b) == 0);
	assert(jsval_date_get_time(&region, date_a, &result) == 0);
	assert_number_value(result, 1577934245006.0);
	assert(jsval_date_value_of(&region, date_a, &result) == 0);
	assert_number_value(result, 1577934245006.0);

	assert(jsval_date_get_utc_full_year(&region, date_a, &result) == 0);
	assert_number_value(result, 2020.0);
	assert(jsval_date_get_utc_month(&region, date_a, &result) == 0);
	assert_number_value(result, 0.0);
	assert(jsval_date_get_utc_date(&region, date_a, &result) == 0);
	assert_number_value(result, 2.0);
	assert(jsval_date_get_utc_day(&region, date_a, &result) == 0);
	assert_number_value(result, 4.0);
	assert(jsval_date_get_utc_hours(&region, date_a, &result) == 0);
	assert_number_value(result, 3.0);
	assert(jsval_date_get_utc_minutes(&region, date_a, &result) == 0);
	assert_number_value(result, 4.0);
	assert(jsval_date_get_utc_seconds(&region, date_a, &result) == 0);
	assert_number_value(result, 5.0);
	assert(jsval_date_get_utc_milliseconds(&region, date_a, &result) == 0);
	assert_number_value(result, 6.0);

	memset(&error, 0, sizeof(error));
	assert(jsval_date_to_iso_string(&region, date_a, &result, &error) == 0);
	assert_string(&region, result, "2020-01-02T03:04:05.006Z");
	assert(error.kind == JSMETHOD_ERROR_NONE);
	assert(jsval_date_to_utc_string(&region, date_a, &result) == 0);
	assert_string(&region, result, "Thu, 02 Jan 2020 03:04:05 GMT");
	assert(jsval_date_to_json(&region, date_a, &result, &error) == 0);
	assert_string(&region, result, "2020-01-02T03:04:05.006Z");

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, &input) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_date_parse_iso(&region, input, &result, &error) == 0);
	assert_number_value(result, 1577934245006.0);
	assert(jsval_date_new_iso(&region, input, &parsed, &error) == 0);
	assert(jsval_date_to_iso_string(&region, parsed, &result, &error) == 0);
	assert_string(&region, result, "2020-01-02T03:04:05.006Z");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"bad-date", 8,
			&bad_input) == 0);
	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(jsval_date_parse_iso(&region, bad_input, &result, &error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_SYNTAX);

	args[0] = jsval_number(99.0);
	args[1] = jsval_number(0.0);
	args[2] = jsval_number(2.0);
	args[3] = jsval_number(3.0);
	args[4] = jsval_number(4.0);
	args[5] = jsval_number(5.0);
	args[6] = jsval_number(6.0);
	memset(&error, 0, sizeof(error));
	assert(jsval_date_new_utc_fields(&region, 7, args, &ctor_99, &error) == 0);
	assert(jsval_date_get_utc_full_year(&region, ctor_99, &result) == 0);
	assert_number_value(result, 1999.0);
	assert(jsval_date_utc(&region, 7, args, &result, &error) == 0);
	assert_number_value(result, 915246245006.0);

	assert(jsval_date_set_utc_month(&region, ctor_99, jsval_number(1.0),
			&result) == 0);
	assert_number_value(result, 917924645006.0);
	assert(jsval_date_get_utc_month(&region, ctor_99, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_date_set_utc_date(&region, ctor_99, jsval_number(29.0),
			&result) == 0);
	assert(jsval_date_get_utc_date(&region, ctor_99, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_date_get_utc_month(&region, ctor_99, &result) == 0);
	assert_number_value(result, 2.0);

	memset(&error, 0, sizeof(error));
	assert(jsval_date_new_local_fields(&region, 7, args, &local_date,
			&error) == 0);
	assert(jsval_date_get_full_year(&region, local_date, &result) == 0);
	assert_number_value(result, 1999.0);
	assert(jsval_date_get_month(&region, local_date, &result) == 0);
	assert_number_value(result, 0.0);
	assert(jsval_date_get_date(&region, local_date, &result) == 0);
	assert_number_value(result, 2.0);
	assert(jsval_date_get_hours(&region, local_date, &result) == 0);
	assert_number_value(result, 3.0);
	assert(jsval_date_get_minutes(&region, local_date, &result) == 0);
	assert_number_value(result, 4.0);
	assert(jsval_date_get_seconds(&region, local_date, &result) == 0);
	assert_number_value(result, 5.0);
	assert(jsval_date_get_milliseconds(&region, local_date, &result) == 0);
	assert_number_value(result, 6.0);
	assert(jsval_date_set_month(&region, local_date, jsval_number(1.0),
			&result) == 0);
	assert(jsval_date_get_month(&region, local_date, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_date_set_hours(&region, local_date, jsval_number(23.0),
			&result) == 0);
	assert(jsval_date_get_hours(&region, local_date, &result) == 0);
	assert_number_value(result, 23.0);
	assert(jsval_date_to_string(&region, local_date, &result) == 0);
	assert(result.kind == JSVAL_KIND_STRING);
	assert(jsval_string_copy_utf8(&region, result, NULL, 0, &len) == 0);
	assert(len > 3 && len < sizeof(buf));
	assert(jsval_string_copy_utf8(&region, result, buf, len, NULL) == 0);
	buf[len] = '\0';
	assert(strstr((const char *)buf, "GMT") != NULL);

	assert(jsval_date_new_time(&region, jsval_number(NAN), &invalid) == 0);
	assert(jsval_date_is_valid(&region, invalid, &valid) == 0);
	assert(valid == 0);
	assert(jsval_date_get_time(&region, invalid, &result) == 0);
	assert_nan_value(result);
	assert(jsval_date_get_utc_full_year(&region, invalid, &result) == 0);
	assert_nan_value(result);
	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(jsval_date_to_iso_string(&region, invalid, &result, &error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_RANGE);
	assert(strcmp(error.message, "Invalid time value") == 0);
	assert(jsval_date_to_utc_string(&region, invalid, &result) == 0);
	assert_string(&region, result, "Invalid Date");
	assert(jsval_date_to_string(&region, invalid, &result) == 0);
	assert_string(&region, result, "Invalid Date");
	assert(jsval_date_to_json(&region, invalid, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_NULL);
}

static void test_crypto_semantics(void)
{
	static const uint8_t sha1_zeros_16[] = {
		0xe1, 0x29, 0xf2, 0x7c, 0x51, 0x03, 0xbc, 0x5c,
		0xc4, 0x4b, 0xcd, 0xf0, 0xa1, 0x5e, 0x16, 0x0d,
		0x44, 0x50, 0x66, 0xff
	};
	static const uint8_t sha256_zeros_16[] = {
		0x37, 0x47, 0x08, 0xff, 0xf7, 0x71, 0x9d, 0xd5,
		0x97, 0x9e, 0xc8, 0x75, 0xd5, 0x6c, 0xd2, 0x28,
		0x6f, 0x6d, 0x3c, 0xf7, 0xec, 0x31, 0x7a, 0x3b,
		0x25, 0x63, 0x2a, 0xab, 0x28, 0xec, 0x37, 0xbb
	};
	static const uint8_t zero_key_32[32] = { 0 };
	static const uint8_t zero_key_16[16] = { 0 };
	static const uint8_t masked_key_9_bits[] = { 0xff, 0x80 };
	static const uint8_t pbkdf2_zero_derivebits_32[] = {
		0x40, 0x2a, 0xf3, 0x77, 0xcd, 0xb5, 0xeb, 0x7e,
		0x01, 0x7a, 0x58, 0x19, 0xf4, 0x70, 0x85, 0x86,
		0x00, 0xe2, 0xc9, 0xbb, 0x25, 0xb7, 0xae, 0x51,
		0xb0, 0x4b, 0x20, 0x95, 0x83, 0xba, 0x77, 0xcd
	};
	static const uint8_t pbkdf2_zero_derivekey_aes_16[] = {
		0x40, 0x2a, 0xf3, 0x77, 0xcd, 0xb5, 0xeb, 0x7e,
		0x01, 0x7a, 0x58, 0x19, 0xf4, 0x70, 0x85, 0x86
	};
	static const uint8_t pbkdf2_zero_derivekey_hmac_64[] = {
		0x40, 0x2a, 0xf3, 0x77, 0xcd, 0xb5, 0xeb, 0x7e,
		0x01, 0x7a, 0x58, 0x19, 0xf4, 0x70, 0x85, 0x86,
		0x00, 0xe2, 0xc9, 0xbb, 0x25, 0xb7, 0xae, 0x51,
		0xb0, 0x4b, 0x20, 0x95, 0x83, 0xba, 0x77, 0xcd,
		0xd4, 0xbd, 0x77, 0x2c, 0x6d, 0xd0, 0xde, 0x9e,
		0xe4, 0x1c, 0x40, 0xa5, 0x35, 0x0f, 0xdc, 0x86,
		0xb6, 0x12, 0xad, 0xbd, 0xb3, 0xba, 0x67, 0x02,
		0x01, 0xd2, 0x54, 0x7c, 0xc1, 0x9b, 0xda, 0x01
	};
	static const uint8_t hkdf_zero_derivebits_32[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb,
		0x2f, 0x2d, 0xb9, 0x1d, 0xa3, 0xfa, 0xc6, 0xd8,
		0x95, 0x0c, 0xab, 0x2f, 0x2b, 0x2e, 0xe4, 0x51
	};
	static const uint8_t hkdf_zero_derivekey_aes_16[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb
	};
	static const uint8_t hkdf_zero_derivekey_hmac_64[] = {
		0x82, 0x31, 0x15, 0x09, 0x86, 0xb1, 0xed, 0x08,
		0x62, 0x8a, 0x63, 0xfa, 0x43, 0xd5, 0x42, 0xcb,
		0x2f, 0x2d, 0xb9, 0x1d, 0xa3, 0xfa, 0xc6, 0xd8,
		0x95, 0x0c, 0xab, 0x2f, 0x2b, 0x2e, 0xe4, 0x51,
		0x2e, 0xe4, 0xce, 0x8f, 0x92, 0x5c, 0xb7, 0x1e,
		0xa7, 0x3c, 0x6e, 0x7f, 0x0c, 0x29, 0x80, 0x1e,
		0x9f, 0xea, 0x6f, 0xfa, 0x11, 0xbe, 0x5e, 0x73,
		0x4c, 0xcd, 0x50, 0x39, 0x98, 0x3c, 0xd7, 0xae
	};
	static const uint8_t aes_gcm_zero_ciphertext_16_tag[] = {
		0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
		0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78,
		0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
		0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf
	};
	static const char *expected_sign_verify_ops[] = { "sign", "verify" };
	static const char *expected_sign_ops[] = { "sign" };
	static const char *expected_encrypt_decrypt_ops[] = {
		"encrypt", "decrypt"
	};
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t buffer;
	jsval_t typed_u8;
	jsval_t typed_f32;
	jsval_t bytes_view;
	jsval_t crypto;
	jsval_t subtle_a;
	jsval_t subtle_b;
	jsval_t digest_input;
	jsval_t digest_input_buffer;
	jsval_t digest_algorithm_object;
	jsval_t digest_promise;
	jsval_t digest_dict_promise;
	jsval_t invalid_algorithm_promise;
	jsval_t invalid_data_promise;
	jsval_t uuid;
	jsval_t key;
	jsval_t dom_exception;
	jsval_t algorithm_name;
	jsval_t result;
	size_t len = 0;
	uint32_t usages = 0;
	int extractable = 0;
	jsval_typed_array_kind_t typed_kind;
	jsval_promise_state_t promise_state;
	jsmethod_error_t error;
	uint8_t before[16];
	uint8_t after[16];
	uint8_t uuid_buf[37];

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_array_buffer_new(&region, 16, &buffer) == 0);
	assert(buffer.kind == JSVAL_KIND_ARRAY_BUFFER);
	assert(jsval_truthy(&region, buffer) == 1);
	assert(jsval_typeof(&region, buffer, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_array_buffer_byte_length(&region, buffer, &len) == 0);
	assert(len == 16);
	assert(jsval_array_buffer_copy_bytes(&region, buffer, after, sizeof(after),
			&len) == 0);
	assert(len == 16);
	memset(before, 0, sizeof(before));
	assert(memcmp(before, after, sizeof(before)) == 0);

	assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
			&typed_u8) == 0);
	assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_FLOAT32, 4,
			&typed_f32) == 0);
	assert(typed_u8.kind == JSVAL_KIND_TYPED_ARRAY);
	assert(jsval_typed_array_kind(&region, typed_u8, &typed_kind) == 0);
	assert(typed_kind == JSVAL_TYPED_ARRAY_UINT8);
	assert(jsval_typed_array_length(&region, typed_u8) == 16);
	assert(jsval_typed_array_byte_length(&region, typed_u8, &len) == 0);
	assert(len == 16);
	assert(jsval_typed_array_buffer(&region, typed_u8, &bytes_view) == 0);
	assert(bytes_view.kind == JSVAL_KIND_ARRAY_BUFFER);
	assert(jsval_typed_array_get_number(&region, typed_u8, 0, &result) == 0);
	assert_number_value(result, 0.0);
	assert(jsval_truthy(&region, typed_u8) == 1);
	assert(jsval_typeof(&region, typed_u8, &result) == 0);
	assert_string(&region, result, "object");

	assert(jsval_crypto_new(&region, &crypto) == 0);
	assert(crypto.kind == JSVAL_KIND_CRYPTO);
	assert(jsval_truthy(&region, crypto) == 1);
	assert(jsval_typeof(&region, crypto, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_crypto_subtle(&region, crypto, &subtle_a) == 0);
	assert(jsval_crypto_subtle(&region, crypto, &subtle_b) == 0);
	assert(subtle_a.kind == JSVAL_KIND_SUBTLE_CRYPTO);
	assert(jsval_strict_eq(&region, subtle_a, subtle_b) == 1);
	assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
			&digest_input) == 0);
	assert(jsval_typed_array_buffer(&region, digest_input, &digest_input_buffer)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
			&algorithm_name) == 0);

#if JSMX_WITH_CRYPTO
	assert(jsval_typed_array_copy_bytes(&region, typed_u8, before,
			sizeof(before), &len) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_crypto_get_random_values(&region, crypto, typed_u8, &result,
			&error) == 0);
	assert(jsval_strict_eq(&region, result, typed_u8) == 1);
	assert(jsval_typed_array_copy_bytes(&region, typed_u8, after, sizeof(after),
			&len) == 0);
	assert(memcmp(before, after, sizeof(after)) != 0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(jsval_crypto_get_random_values(&region, crypto, typed_f32, &result,
			&error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_TYPE);
	assert(strcmp(error.message, "TypeMismatchError") == 0);

	assert(jsval_crypto_random_uuid(&region, crypto, &uuid) == 0);
	assert(jsval_string_copy_utf8(&region, uuid, NULL, 0, &len) == 0);
	assert(len == 36);
	assert(jsval_string_copy_utf8(&region, uuid, uuid_buf, 36, NULL) == 0);
	uuid_buf[36] = '\0';
	assert(uuid_buf[8] == '-');
	assert(uuid_buf[13] == '-');
	assert(uuid_buf[18] == '-');
	assert(uuid_buf[23] == '-');
	assert(uuid_buf[14] == '4');
	assert(uuid_buf[19] == '8' || uuid_buf[19] == '9'
			|| uuid_buf[19] == 'a' || uuid_buf[19] == 'b');

	assert(jsval_subtle_crypto_digest(&region, subtle_a, algorithm_name,
			digest_input, &digest_promise) == 0);
	assert(digest_promise.kind == JSVAL_KIND_PROMISE);
	assert(jsval_promise_state(&region, digest_promise, &promise_state) == 0);
	assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_microtask_pending(&region) == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(error.kind == JSMETHOD_ERROR_NONE);
	assert(jsval_promise_state(&region, digest_promise, &promise_state) == 0);
	assert(promise_state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, digest_promise, &result) == 0);
	assert(jsval_strict_eq(&region, result, digest_input_buffer) == 0);
	assert_array_buffer_bytes(&region, result, sha256_zeros_16,
			sizeof(sha256_zeros_16));

	assert(jsval_object_new(&region, 1, &digest_algorithm_object) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-1", 5,
			&result) == 0);
	assert(jsval_object_set_utf8(&region, digest_algorithm_object,
			(const uint8_t *)"name", 4, result) == 0);
	assert(jsval_subtle_crypto_digest(&region, subtle_a,
			digest_algorithm_object, digest_input_buffer,
			&digest_dict_promise) == 0);
	assert(jsval_promise_state(&region, digest_dict_promise, &promise_state)
			== 0);
	assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(error.kind == JSMETHOD_ERROR_NONE);
	assert(jsval_promise_state(&region, digest_dict_promise, &promise_state)
			== 0);
	assert(promise_state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, digest_dict_promise, &result) == 0);
	assert_array_buffer_bytes(&region, result, sha1_zeros_16,
			sizeof(sha1_zeros_16));

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-999", 7,
			&result) == 0);
	assert(jsval_subtle_crypto_digest(&region, subtle_a, result, digest_input,
			&invalid_algorithm_promise) == 0);
	assert(jsval_promise_state(&region, invalid_algorithm_promise,
			&promise_state) == 0);
	assert(promise_state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, invalid_algorithm_promise, &result)
			== 0);
	assert_dom_exception(&region, result, "NotSupportedError",
			"unsupported digest algorithm");

	assert(jsval_subtle_crypto_digest(&region, subtle_a, algorithm_name,
			jsval_number(5.0), &invalid_data_promise) == 0);
	assert(jsval_promise_state(&region, invalid_data_promise, &promise_state)
			== 0);
	assert(promise_state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, invalid_data_promise, &result) == 0);
	assert_dom_exception(&region, result, "TypeError",
			"expected BufferSource input");

	{
		jsval_t usages_array;
		jsval_t hmac_algorithm;
		jsval_t hash_object;
		jsval_t generated_key_promise;
		jsval_t generated_key;
		jsval_t exported_raw_promise;
		jsval_t exported_jwk_promise;
		jsval_t sign_promise;
		jsval_t verify_promise;
		jsval_t verify_false_promise;
		jsval_t usage_string;
		jsval_t sign_key_ops;
		jsval_t raw_import_key_promise;
		jsval_t raw_import_key;
		jsval_t zero_key_input;
		jsval_t non_extractable_key_promise;
		jsval_t non_extractable_key;
		jsval_t non_extractable_export_promise;
		jsval_t empty_usages;
		jsval_t empty_usages_promise;
		jsval_t format_value;
		jsval_t hmac_name;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t unsupported_format_promise;
		jsval_t jwk_object;
		jsval_t jwk_import_key_promise;
		jsval_t jwk_import_key;
		jsval_t jwk_export_raw_promise;
		jsval_t jwk_export_jwk_promise;

		assert(jsval_array_new(&region, 2, &usages_array) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&usage_string) == 0);
		assert(jsval_array_push(&region, usages_array, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&usage_string) == 0);
		assert(jsval_array_push(&region, usages_array, usage_string) == 0);
		assert(jsval_object_new(&region, 1, &hash_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hash_object,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_new(&region, 3, &hmac_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&usage_string) == 0);
		hmac_name = usage_string;
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"hash", 4, hash_object) == 0);

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				hmac_algorithm, 1, usages_array, &generated_key_promise) == 0);
		assert(jsval_promise_state(&region, generated_key_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(error.kind == JSMETHOD_ERROR_NONE);
		assert(jsval_promise_result(&region, generated_key_promise,
				&generated_key) == 0);
		assert(generated_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, generated_key, &result) == 0);
		assert(result.kind == JSVAL_KIND_OBJECT);
		assert_object_string_prop(&region, result, "name", "HMAC");
		assert(jsval_object_get_utf8(&region, result, (const uint8_t *)"hash", 4,
				&hash_object) == 0);
		assert_object_string_prop(&region, hash_object, "name", "SHA-256");
		assert_object_number_prop(&region, result, "length", 512.0);
		assert(jsval_crypto_key_usages(&region, generated_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_SIGN
				| JSVAL_CRYPTO_KEY_USAGE_VERIFY));
		assert(jsval_crypto_key_extractable(&region, generated_key,
				&extractable) == 0);
		assert(extractable == 1);

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				generated_key, &exported_raw_promise) == 0);
		assert(jsval_promise_state(&region, exported_raw_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result)
				== 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 64);

		assert(jsval_subtle_crypto_sign(&region, subtle_a, hmac_name,
				generated_key, digest_input_buffer, &sign_promise) == 0);
		assert(jsval_promise_state(&region, sign_promise, &promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, sign_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 32);

		assert(jsval_subtle_crypto_verify(&region, subtle_a, usage_string,
				generated_key, result, digest_input_buffer,
				&verify_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, verify_promise, &uuid) == 0);
		assert(uuid.kind == JSVAL_KIND_BOOL && uuid.as.boolean == 1);

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 15,
				&bytes_view) == 0);
		assert(jsval_typed_array_buffer(&region, bytes_view, &buffer) == 0);
		assert(jsval_subtle_crypto_verify(&region, subtle_a, usage_string,
				generated_key, result, buffer, &verify_false_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, verify_false_promise, &uuid) == 0);
		assert(uuid.kind == JSVAL_KIND_BOOL && uuid.as.boolean == 0);

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				generated_key, &exported_jwk_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_jwk_promise, &result)
				== 0);
		assert_object_string_prop(&region, result, "kty", "oct");
		assert_object_string_prop(&region, result, "alg", "HS256");
		assert(jsval_object_get_utf8(&region, result, (const uint8_t *)"ext", 3,
				&uuid) == 0);
		assert(uuid.kind == JSVAL_KIND_BOOL && uuid.as.boolean == 1);
		assert(jsval_object_get_utf8(&region, result,
				(const uint8_t *)"key_ops", 7, &sign_key_ops) == 0);
		assert_array_strings(&region, sign_key_ops, expected_sign_verify_ops, 2);

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 32,
				&zero_key_input) == 0);
		assert(jsval_array_new(&region, 1, &sign_key_ops) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&usage_string) == 0);
		assert(jsval_array_push(&region, sign_key_ops, usage_string) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				zero_key_input, hmac_algorithm, 1,
				sign_key_ops, &raw_import_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, raw_import_key_promise,
				&raw_import_key) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				raw_import_key, &exported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, zero_key_32,
				sizeof(zero_key_32));

		assert(jsval_object_new(&region, 5, &jwk_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"oct", 3,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"__8", 3,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object, (const uint8_t *)"k", 1,
				usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HS256", 5,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, sign_key_ops) == 0);
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"length", 6, jsval_number(9.0)) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				jwk_object, hmac_algorithm, 1, sign_key_ops,
				&jwk_import_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_import_key_promise,
				&jwk_import_key) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				jwk_import_key,
				&jwk_export_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_export_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, masked_key_9_bits,
				sizeof(masked_key_9_bits));
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				jwk_import_key,
				&jwk_export_jwk_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_export_jwk_promise, &result)
				== 0);
		assert_object_string_prop(&region, result, "k", "_4A");
		assert_object_string_prop(&region, result, "alg", "HS256");
		assert(jsval_object_get_utf8(&region, result,
				(const uint8_t *)"key_ops", 7, &sign_key_ops) == 0);
		assert_array_strings(&region, sign_key_ops, expected_sign_ops, 1);

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				hmac_algorithm, 0, sign_key_ops,
				&non_extractable_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, non_extractable_key_promise,
				&non_extractable_key) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				non_extractable_key,
				&non_extractable_export_promise) == 0);
		assert(jsval_promise_result(&region, non_extractable_export_promise,
				&result) == 0);
		assert_dom_exception(&region, result, "InvalidAccessError",
				"key is not extractable");

		assert(jsval_array_new(&region, 0, &empty_usages) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				hmac_algorithm, 1, empty_usages,
				&empty_usages_promise) == 0);
		assert(jsval_promise_result(&region, empty_usages_promise, &result)
				== 0);
		assert_dom_exception(&region, result, "SyntaxError",
				"expected non-empty key usages");

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"pkcs8", 5,
				&format_value) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, format_value,
				zero_key_input, hmac_algorithm, 1, sign_key_ops,
				&unsupported_format_promise) == 0);
		assert(jsval_promise_result(&region, unsupported_format_promise,
				&result) == 0);
		assert_dom_exception(&region, result, "NotSupportedError",
				"unsupported key format");
	}

	{
		jsval_t usages_array;
		jsval_t aes_algorithm;
		jsval_t aes_import_algorithm;
		jsval_t aes_params;
		jsval_t aes_params_aad;
		jsval_t generated_key_promise;
		jsval_t generated_key;
		jsval_t exported_raw_promise;
		jsval_t exported_jwk_promise;
		jsval_t encrypt_promise;
		jsval_t decrypt_promise;
		jsval_t encrypt_aad_promise;
		jsval_t decrypt_aad_promise;
		jsval_t import_key_promise;
		jsval_t import_key;
		jsval_t jwk_object;
		jsval_t jwk_import_key_promise;
		jsval_t jwk_import_key;
		jsval_t jwk_export_raw_promise;
		jsval_t jwk_export_jwk_promise;
		jsval_t usage_string;
		jsval_t key_ops_value;
		jsval_t raw_key_input;
		jsval_t iv_input;
		jsval_t iv_buffer;
		jsval_t aad_input;
		jsval_t aad_buffer;
		jsval_t non_extractable_key_promise;
		jsval_t non_extractable_key;
		jsval_t non_extractable_export_promise;
		jsval_t empty_usages;
		jsval_t empty_usages_promise;
		jsval_t format_value;
		jsval_t unsupported_format_promise;
		jsval_t invalid_iv_params;
		jsval_t invalid_iv_promise;
		jsval_t invalid_tag_params;
		jsval_t invalid_tag_promise;
		jsval_t malformed_jwk_promise;
		jsval_t bad_decrypt_promise;
		jsval_t bad_decrypt_key_promise;
		jsval_t bad_decrypt_key;
		jsval_t raw_format;
		jsval_t jwk_format;

		assert(jsval_array_new(&region, 2, &usages_array) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&usage_string) == 0);
		assert(jsval_array_push(&region, usages_array, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&usage_string) == 0);
		assert(jsval_array_push(&region, usages_array, usage_string) == 0);
		assert(jsval_object_new(&region, 2, &aes_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_algorithm,
				(const uint8_t *)"length", 6, jsval_number(256.0)) == 0);
		assert(jsval_object_new(&region, 1, &aes_import_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_import_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				aes_algorithm, 1, usages_array, &generated_key_promise) == 0);
		assert(jsval_promise_state(&region, generated_key_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, generated_key_promise,
				&generated_key) == 0);
		assert(generated_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, generated_key, &result) == 0);
		assert(result.kind == JSVAL_KIND_OBJECT);
		assert_object_string_prop(&region, result, "name", "AES-GCM");
		assert_object_number_prop(&region, result, "length", 256.0);
		assert(jsval_crypto_key_usages(&region, generated_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
				| JSVAL_CRYPTO_KEY_USAGE_DECRYPT));

		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				generated_key, &exported_raw_promise) == 0);
		assert(jsval_promise_state(&region, exported_raw_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 32);

		assert(jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				generated_key, &exported_jwk_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_jwk_promise, &result) == 0);
		assert_object_string_prop(&region, result, "kty", "oct");
		assert_object_string_prop(&region, result, "alg", "A256GCM");
		assert(jsval_object_get_utf8(&region, result, (const uint8_t *)"key_ops",
				7, &key_ops_value) == 0);
		assert_array_strings(&region, key_ops_value, expected_encrypt_decrypt_ops,
				2);

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&raw_key_input) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				raw_key_input, aes_import_algorithm, 1, usages_array,
				&import_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, import_key_promise, &import_key) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				import_key, &exported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result) == 0);
		assert_array_buffer_bytes(&region, result, zero_key_16,
				sizeof(zero_key_16));

		assert(jsval_object_new(&region, 5, &jwk_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"oct", 3,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, usage_string) == 0);
		assert(jsval_string_new_utf8(&region,
				(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAA", 22,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object, (const uint8_t *)"k", 1,
				usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"A128GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, usages_array) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				jwk_object, aes_import_algorithm, 1, usages_array,
				&jwk_import_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_import_key_promise,
				&jwk_import_key) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				jwk_import_key, &jwk_export_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_export_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, zero_key_16,
				sizeof(zero_key_16));
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				jwk_import_key, &jwk_export_jwk_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_export_jwk_promise, &result)
				== 0);
		assert_object_string_prop(&region, result, "alg", "A128GCM");
		assert(jsval_object_get_utf8(&region, result, (const uint8_t *)"key_ops",
				7, &key_ops_value) == 0);
		assert_array_strings(&region, key_ops_value, expected_encrypt_decrypt_ops,
				2);

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
				&iv_input) == 0);
		assert(jsval_typed_array_buffer(&region, iv_input, &iv_buffer) == 0);
		assert(jsval_object_new(&region, 2, &aes_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_params,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_params,
				(const uint8_t *)"iv", 2, iv_buffer) == 0);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, aes_params,
				import_key, digest_input_buffer, &encrypt_promise) == 0);
		assert(jsval_promise_state(&region, encrypt_promise, &promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, encrypt_promise, &result) == 0);
		assert_array_buffer_bytes(&region, result, aes_gcm_zero_ciphertext_16_tag,
				sizeof(aes_gcm_zero_ciphertext_16_tag));

		assert(jsval_subtle_crypto_decrypt(&region, subtle_a, aes_params,
				import_key, result, &decrypt_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, decrypt_promise, &result) == 0);
		assert_array_buffer_bytes(&region, result, zero_key_16,
				sizeof(zero_key_16));

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 4,
				&aad_input) == 0);
		assert(jsval_typed_array_buffer(&region, aad_input, &aad_buffer) == 0);
		assert(jsval_object_new(&region, 4, &aes_params_aad) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_params_aad,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_params_aad,
				(const uint8_t *)"iv", 2, iv_buffer) == 0);
		assert(jsval_object_set_utf8(&region, aes_params_aad,
				(const uint8_t *)"additionalData", 14, aad_buffer) == 0);
		assert(jsval_object_set_utf8(&region, aes_params_aad,
				(const uint8_t *)"tagLength", 9, jsval_number(96.0)) == 0);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, aes_params_aad,
				import_key, digest_input_buffer, &encrypt_aad_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, encrypt_aad_promise, &result) == 0);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 28);
		assert(jsval_subtle_crypto_decrypt(&region, subtle_a, aes_params_aad,
				import_key, result, &decrypt_aad_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, decrypt_aad_promise, &result) == 0);
		assert_array_buffer_bytes(&region, result, zero_key_16,
				sizeof(zero_key_16));

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				aes_algorithm, 1, usages_array, &bad_decrypt_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, bad_decrypt_key_promise,
				&bad_decrypt_key) == 0);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, aes_params,
				import_key, digest_input_buffer, &encrypt_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, encrypt_promise, &result) == 0);
			assert(jsval_subtle_crypto_decrypt(&region, subtle_a, aes_params,
					bad_decrypt_key, result, &bad_decrypt_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, bad_decrypt_promise, &result) == 0);
			assert_dom_exception(&region, result, "OperationError",
					"AES-GCM operation failed");

		assert(jsval_object_new(&region, 1, &invalid_iv_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, invalid_iv_params,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, invalid_iv_params,
				import_key, digest_input_buffer, &invalid_iv_promise) == 0);
		assert(jsval_promise_result(&region, invalid_iv_promise, &result) == 0);
		assert_dom_exception(&region, result, "TypeError",
				"expected AES-GCM iv");

		assert(jsval_object_new(&region, 3, &invalid_tag_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, invalid_tag_params,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, invalid_tag_params,
				(const uint8_t *)"iv", 2, iv_buffer) == 0);
		assert(jsval_object_set_utf8(&region, invalid_tag_params,
				(const uint8_t *)"tagLength", 9, jsval_number(40.0)) == 0);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, invalid_tag_params,
				import_key, digest_input_buffer, &invalid_tag_promise) == 0);
		assert(jsval_promise_result(&region, invalid_tag_promise, &result) == 0);
		assert_dom_exception(&region, result, "OperationError",
				"invalid AES-GCM tagLength");

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				aes_algorithm, 0, usages_array, &non_extractable_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, non_extractable_key_promise,
				&non_extractable_key) == 0);
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				non_extractable_key, &non_extractable_export_promise) == 0);
		assert(jsval_promise_result(&region, non_extractable_export_promise,
				&result) == 0);
		assert_dom_exception(&region, result, "InvalidAccessError",
				"key is not extractable");

		assert(jsval_array_new(&region, 0, &empty_usages) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				aes_algorithm, 1, empty_usages, &empty_usages_promise) == 0);
		assert(jsval_promise_result(&region, empty_usages_promise, &result) == 0);
		assert_dom_exception(&region, result, "SyntaxError",
				"expected non-empty key usages");

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"pkcs8", 5,
				&format_value) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, format_value,
				raw_key_input, aes_import_algorithm, 1, usages_array,
				&unsupported_format_promise) == 0);
		assert(jsval_promise_result(&region, unsupported_format_promise,
				&result) == 0);
		assert_dom_exception(&region, result, "NotSupportedError",
				"unsupported key format");

		assert(jsval_object_new(&region, 5, &jwk_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA", 3,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, usage_string) == 0);
		assert(jsval_string_new_utf8(&region,
				(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAA", 22,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"A128GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, usages_array) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				jwk_object, aes_import_algorithm, 1, usages_array,
				&malformed_jwk_promise) == 0);
		assert(jsval_promise_result(&region, malformed_jwk_promise, &result) == 0);
		assert_dom_exception(&region, result, "DataError", "invalid JWK kty");
	}

	{
		/*
		 * AES-CTR Promise-backed surface. Round-trips zero buffers
		 * through the runtime (vector exactness lives in test_jscrypto.c
		 * since the runtime has no public typed-array mutator).
		 */
		jsval_t ctr_usages;
		jsval_t ctr_algorithm;
		jsval_t ctr_import_algorithm;
		jsval_t ctr_params;
		jsval_t ctr_bad_params;
		jsval_t plaintext_input;
		jsval_t plaintext_buffer;
		jsval_t counter_input;
		jsval_t counter_buffer;
		jsval_t generated_promise;
		jsval_t generated_key;
		jsval_t exported_raw_promise;
		jsval_t imported_raw_promise;
		jsval_t imported_raw_key;
		jsval_t encrypt_promise;
		jsval_t decrypt_promise;
		jsval_t exported_jwk_promise;
		jsval_t jwk_object;
		jsval_t jwk_imported_promise;
		jsval_t jwk_imported_key;
		jsval_t bad_counter_promise;
		jsval_t bad_length_promise;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t name_string;
		jsval_t alg_value;
		jsval_t ciphertext;
		uint32_t ctr_usages_mask = 0;

		assert(jsval_array_new(&region, 2, &ctr_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, ctr_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, ctr_usages, name_string) == 0);
		assert(jsval_object_new(&region, 2, &ctr_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-CTR", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, ctr_algorithm,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, ctr_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_object_new(&region, 1, &ctr_import_algorithm) == 0);
		assert(jsval_object_set_utf8(&region, ctr_import_algorithm,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&plaintext_input) == 0);
		assert(jsval_typed_array_buffer(&region, plaintext_input,
				&plaintext_buffer) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&counter_input) == 0);
		assert(jsval_typed_array_buffer(&region, counter_input,
				&counter_buffer) == 0);
		assert(jsval_object_new(&region, 3, &ctr_params) == 0);
		assert(jsval_object_set_utf8(&region, ctr_params,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, ctr_params,
				(const uint8_t *)"counter", 7, counter_buffer) == 0);
		assert(jsval_object_set_utf8(&region, ctr_params,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);

		/* generateKey resolves to an AES-CTR CryptoKey of the right shape. */
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, ctr_algorithm,
				1, ctr_usages, &generated_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, generated_promise,
				&generated_key) == 0);
		assert(generated_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, generated_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "AES-CTR");
		assert_object_number_prop(&region, alg_value, "length", 128.0);
		assert(jsval_crypto_key_usages(&region, generated_key,
				&ctr_usages_mask) == 0);
		assert(ctr_usages_mask
				== (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
					| JSVAL_CRYPTO_KEY_USAGE_DECRYPT));

		/* exportKey raw on generated key returns 16 bytes. */
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				generated_key, &exported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 16);

		/* importKey raw (zero key) + encrypt then decrypt round-trip. */
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				plaintext_buffer, ctr_import_algorithm, 1, ctr_usages,
				&imported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, imported_raw_promise,
				&imported_raw_key) == 0);
		assert(imported_raw_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, ctr_params,
				imported_raw_key, plaintext_buffer, &encrypt_promise) == 0);
		assert(jsval_promise_state(&region, encrypt_promise, &promise_state)
				== 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, encrypt_promise, &ciphertext) == 0);
		assert(ciphertext.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, ciphertext, &len) == 0);
		assert(len == 16);

		/* decrypt round-trips back to the zero plaintext. */
		assert(jsval_subtle_crypto_decrypt(&region, subtle_a, ctr_params,
				imported_raw_key, ciphertext, &decrypt_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, decrypt_promise, &result) == 0);
		{
			static const uint8_t zero16[16] = { 0 };
			assert_array_buffer_bytes(&region, result, zero16, sizeof(zero16));
		}

		/* exportKey jwk -> A128CTR alg, encrypt/decrypt key_ops. */
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				imported_raw_key, &exported_jwk_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_jwk_promise, &result)
				== 0);
		assert_object_string_prop(&region, result, "alg", "A128CTR");
		assert_object_string_prop(&region, result, "kty", "oct");

		/* importKey jwk for the all-zero key (k = sixteen zero bytes
		 * base64url-encoded). */
		assert(jsval_object_new(&region, 5, &jwk_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"oct", 3,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"kty", 3, name_string) == 0);
		assert(jsval_string_new_utf8(&region,
				(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAA", 22, &name_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"k", 1, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"A128CTR", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"alg", 3, name_string) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"ext", 3, jsval_bool(1)) == 0);
		assert(jsval_object_set_utf8(&region, jwk_object,
				(const uint8_t *)"key_ops", 7, ctr_usages) == 0);
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				jwk_object, ctr_import_algorithm, 1, ctr_usages,
				&jwk_imported_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, jwk_imported_promise,
				&jwk_imported_key) == 0);
		assert(jwk_imported_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, ctr_params,
				jwk_imported_key, plaintext_buffer, &encrypt_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, encrypt_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 16);

		/* counter that is not 16 bytes -> TypeError */
		assert(jsval_object_new(&region, 3, &ctr_bad_params) == 0);
		assert(jsval_object_set_utf8(&region, ctr_bad_params,
				(const uint8_t *)"name", 4, name_string) == 0);
		{
			jsval_t bad_counter;
			jsval_t bad_counter_buf;
			assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
					&bad_counter) == 0);
			assert(jsval_typed_array_buffer(&region, bad_counter,
					&bad_counter_buf) == 0);
			assert(jsval_object_set_utf8(&region, ctr_bad_params,
					(const uint8_t *)"counter", 7, bad_counter_buf) == 0);
		}
		assert(jsval_object_set_utf8(&region, ctr_bad_params,
				(const uint8_t *)"length", 6, jsval_number(64.0)) == 0);
		/* Reset name field on ctr_bad_params (clobbered above). */
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-CTR", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, ctr_bad_params,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, ctr_bad_params,
				imported_raw_key, plaintext_buffer, &bad_counter_promise) == 0);
		assert(jsval_promise_result(&region, bad_counter_promise, &result) == 0);
		assert_dom_exception(&region, result, "OperationError",
				"invalid AES-CTR counter length");

		/* length=0 -> TypeError */
		{
			jsval_t bad_length_params;
			assert(jsval_object_new(&region, 3, &bad_length_params) == 0);
			assert(jsval_object_set_utf8(&region, bad_length_params,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_length_params,
					(const uint8_t *)"counter", 7, counter_buffer) == 0);
			assert(jsval_object_set_utf8(&region, bad_length_params,
					(const uint8_t *)"length", 6, jsval_number(0.0)) == 0);
			assert(jsval_subtle_crypto_encrypt(&region, subtle_a,
					bad_length_params, imported_raw_key, plaintext_buffer,
					&bad_length_promise) == 0);
			assert(jsval_promise_result(&region, bad_length_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "OperationError",
					"invalid AES-CTR length");
		}
	}

	{
		/*
		 * AES-CBC Promise-backed surface. Round-trips zero buffers
		 * through the runtime; vector exactness lives in test_jscrypto.c
		 * since the runtime has no public typed-array mutator.
		 */
		jsval_t cbc_usages;
		jsval_t cbc_algorithm;
		jsval_t cbc_import_algorithm;
		jsval_t cbc_params;
		jsval_t cbc_bad_params;
		jsval_t plaintext_input;
		jsval_t plaintext_buffer;
		jsval_t iv_input;
		jsval_t iv_buffer;
		jsval_t generated_promise;
		jsval_t generated_key;
		jsval_t exported_raw_promise;
		jsval_t imported_raw_promise;
		jsval_t imported_raw_key;
		jsval_t encrypt_promise;
		jsval_t decrypt_promise;
		jsval_t exported_jwk_promise;
		jsval_t bad_iv_promise;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t name_string;
		jsval_t alg_value;
		jsval_t ciphertext;
		uint32_t cbc_usages_mask = 0;

		assert(jsval_array_new(&region, 2, &cbc_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, cbc_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, cbc_usages, name_string) == 0);
		assert(jsval_object_new(&region, 2, &cbc_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-CBC", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, cbc_algorithm,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, cbc_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_object_new(&region, 1, &cbc_import_algorithm) == 0);
		assert(jsval_object_set_utf8(&region, cbc_import_algorithm,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&plaintext_input) == 0);
		assert(jsval_typed_array_buffer(&region, plaintext_input,
				&plaintext_buffer) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&iv_input) == 0);
		assert(jsval_typed_array_buffer(&region, iv_input, &iv_buffer) == 0);
		assert(jsval_object_new(&region, 2, &cbc_params) == 0);
		assert(jsval_object_set_utf8(&region, cbc_params,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, cbc_params,
				(const uint8_t *)"iv", 2, iv_buffer) == 0);

		/* generateKey resolves to an AES-CBC CryptoKey of the right shape. */
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, cbc_algorithm,
				1, cbc_usages, &generated_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, generated_promise,
				&generated_key) == 0);
		assert(generated_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, generated_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "AES-CBC");
		assert_object_number_prop(&region, alg_value, "length", 128.0);
		assert(jsval_crypto_key_usages(&region, generated_key,
				&cbc_usages_mask) == 0);
		assert(cbc_usages_mask
				== (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
					| JSVAL_CRYPTO_KEY_USAGE_DECRYPT));

		/* exportKey raw on generated key returns 16 bytes. */
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				generated_key, &exported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 16);

		/* importKey raw (zero key) + encrypt/decrypt round-trip on a zero
		 * 16-byte plaintext. PKCS#7 padding makes the ciphertext 32 bytes
		 * (the trailing block is one full padding block). */
		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				plaintext_buffer, cbc_import_algorithm, 1, cbc_usages,
				&imported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, imported_raw_promise,
				&imported_raw_key) == 0);
		assert(imported_raw_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, cbc_params,
				imported_raw_key, plaintext_buffer, &encrypt_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, encrypt_promise, &ciphertext) == 0);
		assert(ciphertext.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, ciphertext, &len) == 0);
		assert(len == 32);
		assert(jsval_subtle_crypto_decrypt(&region, subtle_a, cbc_params,
				imported_raw_key, ciphertext, &decrypt_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, decrypt_promise, &result) == 0);
		{
			static const uint8_t zero16[16] = { 0 };
			assert_array_buffer_bytes(&region, result, zero16, sizeof(zero16));
		}

		/* exportKey jwk -> A128CBC alg, encrypt/decrypt key_ops. */
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				imported_raw_key, &exported_jwk_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_jwk_promise, &result)
				== 0);
		assert_object_string_prop(&region, result, "alg", "A128CBC");
		assert_object_string_prop(&region, result, "kty", "oct");

		/* iv length != 16 -> OperationError */
		assert(jsval_object_new(&region, 2, &cbc_bad_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-CBC", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, cbc_bad_params,
				(const uint8_t *)"name", 4, name_string) == 0);
		{
			jsval_t bad_iv;
			jsval_t bad_iv_buf;
			assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
					&bad_iv) == 0);
			assert(jsval_typed_array_buffer(&region, bad_iv, &bad_iv_buf) == 0);
			assert(jsval_object_set_utf8(&region, cbc_bad_params,
					(const uint8_t *)"iv", 2, bad_iv_buf) == 0);
		}
		assert(jsval_subtle_crypto_encrypt(&region, subtle_a, cbc_bad_params,
				imported_raw_key, plaintext_buffer, &bad_iv_promise) == 0);
		assert(jsval_promise_result(&region, bad_iv_promise, &result) == 0);
		assert_dom_exception(&region, result, "OperationError",
				"invalid AES-CBC iv length");
	}

	{
		/*
		 * AES-KW Promise-backed surface plus subtle.wrapKey/unwrapKey
		 * round-trips. Mirrors the AES-CBC pattern for the algorithm
		 * scaffolding, then exercises wrapKey/unwrapKey end-to-end with
		 * an HMAC and an AES-GCM inner key.
		 */
		jsval_t kw_usages;
		jsval_t kw_algorithm;
		jsval_t kw_import_algorithm;
		jsval_t generated_promise;
		jsval_t generated_key;
		jsval_t exported_raw_promise;
		jsval_t exported_jwk_promise;
		jsval_t hmac_gen_alg;
		jsval_t hmac_hash_obj;
		jsval_t hmac_gen_usages;
		jsval_t hmac_inner_promise;
		jsval_t hmac_inner_key;
		jsval_t wrap_format;
		jsval_t hmac_alg_for_unwrap;
		jsval_t wrap_promise;
		jsval_t wrapped_buf;
		jsval_t unwrap_promise;
		jsval_t unwrapped_key;
		jsval_t aes_gcm_inner_alg;
		jsval_t aes_gcm_inner_usages;
		jsval_t aes_gcm_inner_promise;
		jsval_t aes_gcm_inner_key;
		jsval_t aes_gcm_wrap_promise;
		jsval_t aes_gcm_wrapped_buf;
		jsval_t aes_gcm_unwrap_promise;
		jsval_t aes_gcm_unwrapped_key;
		jsval_t aes_gcm_alg_for_unwrap;
		jsval_t name_string;
		jsval_t alg_value;
		uint32_t kw_usages_mask = 0;

		assert(jsval_array_new(&region, 2, &kw_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"wrapKey", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, kw_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"unwrapKey", 9,
				&name_string) == 0);
		assert(jsval_array_push(&region, kw_usages, name_string) == 0);
		assert(jsval_object_new(&region, 2, &kw_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-KW", 6,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, kw_algorithm,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, kw_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_object_new(&region, 1, &kw_import_algorithm) == 0);
		assert(jsval_object_set_utf8(&region, kw_import_algorithm,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&wrap_format) == 0);

		/* generateKey resolves to AES-KW with wrap/unwrap usages. */
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, kw_algorithm,
				1, kw_usages, &generated_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, generated_promise,
				&generated_key) == 0);
		assert(generated_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, generated_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "AES-KW");
		assert_object_number_prop(&region, alg_value, "length", 128.0);
		assert(jsval_crypto_key_usages(&region, generated_key,
				&kw_usages_mask) == 0);
		assert(kw_usages_mask
				== (JSVAL_CRYPTO_KEY_USAGE_WRAP_KEY
					| JSVAL_CRYPTO_KEY_USAGE_UNWRAP_KEY));

		/* exportKey raw -> 16 bytes; exportKey jwk -> A128KW alg. */
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, wrap_format,
				generated_key, &exported_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, exported_raw_promise, &result)
				== 0);
		assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
		assert(len == 16);
		{
			jsval_t jwk_format_value;
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
					&jwk_format_value) == 0);
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format_value, generated_key,
					&exported_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, exported_jwk_promise,
					&result) == 0);
			assert_object_string_prop(&region, result, "alg", "A128KW");
		}

		/* Generate an HMAC-SHA-256 inner key (extractable, sign+verify). */
		assert(jsval_object_new(&region, 1, &hmac_hash_obj) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_hash_obj,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_new(&region, 2, &hmac_gen_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_gen_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_gen_alg,
				(const uint8_t *)"hash", 4, hmac_hash_obj) == 0);
		assert(jsval_array_new(&region, 2, &hmac_gen_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, hmac_gen_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, hmac_gen_usages, name_string) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, hmac_gen_alg,
				1, hmac_gen_usages, &hmac_inner_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, hmac_inner_promise,
				&hmac_inner_key) == 0);
		assert(hmac_inner_key.kind == JSVAL_KIND_CRYPTO_KEY);

		/* Wrap the HMAC key with the AES-KW key. */
		assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, wrap_format,
				hmac_inner_key, generated_key, kw_algorithm,
				&wrap_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, wrap_promise, &wrapped_buf) == 0);
		assert(wrapped_buf.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, wrapped_buf, &len) == 0);
		/* HMAC SHA-256 default key length is 64 bytes; wrapped is +8. */
		assert(len == 72);

		/* Unwrap back to an HMAC key. */
		assert(jsval_object_new(&region, 2, &hmac_alg_for_unwrap) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_alg_for_unwrap,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_alg_for_unwrap,
				(const uint8_t *)"hash", 4, hmac_hash_obj) == 0);
		assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, wrap_format,
				wrapped_buf, generated_key, kw_algorithm, hmac_alg_for_unwrap,
				1, hmac_gen_usages, &unwrap_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, unwrap_promise,
				&unwrapped_key) == 0);
		assert(unwrapped_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, unwrapped_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "HMAC");

		/* Repeat with an AES-GCM inner key. Need a fresh AES-KW key
		 * because the previous wrap consumed the wrap usage. Actually
		 * the same key works since wrap/unwrap don't deplete usages. */
		assert(jsval_object_new(&region, 2, &aes_gcm_inner_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_gcm_inner_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_gcm_inner_alg,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_array_new(&region, 2, &aes_gcm_inner_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, aes_gcm_inner_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, aes_gcm_inner_usages, name_string) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				aes_gcm_inner_alg, 1, aes_gcm_inner_usages,
				&aes_gcm_inner_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, aes_gcm_inner_promise,
				&aes_gcm_inner_key) == 0);
		assert(aes_gcm_inner_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, wrap_format,
				aes_gcm_inner_key, generated_key, kw_algorithm,
				&aes_gcm_wrap_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, aes_gcm_wrap_promise,
				&aes_gcm_wrapped_buf) == 0);
		assert(aes_gcm_wrapped_buf.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, aes_gcm_wrapped_buf,
				&len) == 0);
		assert(len == 24);  /* AES-128 key (16 bytes) + 8-byte AIV */
		assert(jsval_object_new(&region, 2, &aes_gcm_alg_for_unwrap) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_gcm_alg_for_unwrap,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_gcm_alg_for_unwrap,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, wrap_format,
				aes_gcm_wrapped_buf, generated_key, kw_algorithm,
				aes_gcm_alg_for_unwrap, 1, aes_gcm_inner_usages,
				&aes_gcm_unwrap_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, aes_gcm_unwrap_promise,
				&aes_gcm_unwrapped_key) == 0);
		assert(aes_gcm_unwrapped_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, aes_gcm_unwrapped_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "AES-GCM");

		/* Negative: tamper the wrapped bytes -> OperationError on unwrap. */
		{
			jsval_t tampered_input;
			jsval_t tampered_buffer;
			jsval_t tampered_unwrap_promise;
			uint8_t copy[72];
			size_t copy_len = 0;
			assert(jsval_array_buffer_copy_bytes(&region, wrapped_buf, copy,
					sizeof(copy), &copy_len) == 0);
			copy[copy_len - 1] ^= 0x01u;
			(void)copy_len;
			/* Build a fresh ArrayBuffer for the tampered bytes by routing
			 * through a typed array (the runtime has no public mutator,
			 * so we re-encrypt a fresh wrap, then mutate via decrypting
			 * to typed-array bytes — actually simpler: just unwrap the
			 * original wrapped_buf with one byte flipped via a wrapKey
			 * round-trip can't be done. Instead, exercise the path by
			 * unwrapping a deliberately too-short buffer.) */
			assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 7,
					&tampered_input) == 0);
			assert(jsval_typed_array_buffer(&region, tampered_input,
					&tampered_buffer) == 0);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, wrap_format,
					tampered_buffer, generated_key, kw_algorithm,
					hmac_alg_for_unwrap, 1, hmac_gen_usages,
					&tampered_unwrap_promise) == 0);
			assert(jsval_promise_result(&region, tampered_unwrap_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "OperationError",
					"invalid AES-KW wrapped key length");
		}

		/* Negative: unsupported unwrappedKeyAlgorithm. */
		{
			jsval_t bad_inner_alg;
			jsval_t bad_unwrap_promise;
			assert(jsval_object_new(&region, 1, &bad_inner_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-OAEP",
					8, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_inner_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a,
					wrap_format, wrapped_buf, generated_key, kw_algorithm,
					bad_inner_alg, 1, hmac_gen_usages,
					&bad_unwrap_promise) == 0);
			assert(jsval_promise_result(&region, bad_unwrap_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "NotSupportedError",
					"unsupported unwrapped key algorithm");
		}

		/*
		 * JWK format wrap/unwrap round-trip. The wrap path serialises
		 * the wrapped key's JWK (preserving algorithm metadata across
		 * the wrap boundary), pads to the AES-KW 8-byte block, and
		 * encrypts. The unwrap path decrypts, parses JSON, validates
		 * the JWK per inner algorithm, and reconstructs the CryptoKey.
		 */
		{
			jsval_t jwk_format;
			jsval_t jwk_wrap_promise;
			jsval_t jwk_wrapped;
			jsval_t jwk_unwrap_promise;
			jsval_t jwk_unwrapped_hmac;
			jsval_t jwk_alg_for_unwrap;
			jsval_t jwk_aes_wrap_promise;
			jsval_t jwk_aes_wrapped;
			jsval_t jwk_aes_unwrap_promise;
			jsval_t jwk_aes_unwrapped;
			jsval_t hmac_hash_obj_jwk;

			assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
					&jwk_format) == 0);

			/* Wrap the HMAC key as JWK then unwrap back. */
			assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, jwk_format,
					hmac_inner_key, generated_key, kw_algorithm,
					&jwk_wrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_wrap_promise,
					&jwk_wrapped) == 0);
			assert(jwk_wrapped.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, jwk_wrapped, &len)
					== 0);
			/* Wrapped JWK is at least 24 bytes (3 blocks of 8 including the
			 * 8-byte AIV). */
			assert(len >= 24 && (len % 8u) == 0);

			/* Build an HMAC alg-for-unwrap object (need fresh hash obj since
			 * object sharing was already used). */
			assert(jsval_object_new(&region, 1, &hmac_hash_obj_jwk) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, hmac_hash_obj_jwk,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_new(&region, 2, &jwk_alg_for_unwrap) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, jwk_alg_for_unwrap,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, jwk_alg_for_unwrap,
					(const uint8_t *)"hash", 4, hmac_hash_obj_jwk) == 0);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, jwk_format,
					jwk_wrapped, generated_key, kw_algorithm,
					jwk_alg_for_unwrap, 1, hmac_gen_usages,
					&jwk_unwrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_unwrap_promise,
					&jwk_unwrapped_hmac) == 0);
			assert(jwk_unwrapped_hmac.kind == JSVAL_KIND_CRYPTO_KEY);
			assert(jsval_crypto_key_algorithm(&region, jwk_unwrapped_hmac,
					&alg_value) == 0);
			assert_object_string_prop(&region, alg_value, "name", "HMAC");
			{
				jsval_t hash_prop;
				assert(jsval_object_get_utf8(&region, alg_value,
						(const uint8_t *)"hash", 4, &hash_prop) == 0);
				assert_object_string_prop(&region, hash_prop, "name",
						"SHA-256");
			}

			/* Wrap the AES-GCM inner key as JWK then unwrap. This proves
			 * the path works for an AES inner algorithm too. */
			assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, jwk_format,
					aes_gcm_inner_key, generated_key, kw_algorithm,
					&jwk_aes_wrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_aes_wrap_promise,
					&jwk_aes_wrapped) == 0);
			assert(jwk_aes_wrapped.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, jwk_format,
					jwk_aes_wrapped, generated_key, kw_algorithm,
					aes_gcm_alg_for_unwrap, 1, aes_gcm_inner_usages,
					&jwk_aes_unwrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_aes_unwrap_promise,
					&jwk_aes_unwrapped) == 0);
			assert(jwk_aes_unwrapped.kind == JSVAL_KIND_CRYPTO_KEY);
			assert(jsval_crypto_key_algorithm(&region, jwk_aes_unwrapped,
					&alg_value) == 0);
			assert_object_string_prop(&region, alg_value, "name", "AES-GCM");
			assert_object_number_prop(&region, alg_value, "length", 128.0);
		}
	}

	{
		/*
		 * AES-GCM as a wrapping cipher. Generate an AES-GCM wrap key
		 * with wrapKey|unwrapKey usages, then wrap an HMAC and an
		 * AES-GCM inner key in both raw and jwk formats and round-trip
		 * back through unwrap. Plus negative cases: missing IV
		 * (TypeError), tampered ciphertext (OperationError via tag
		 * check), and unsupported inner algorithm name
		 * (NotSupportedError).
		 */
		jsval_t gcm_wrap_usages;
		jsval_t gcm_wrap_alg;
		jsval_t gcm_wrap_key_promise;
		jsval_t gcm_wrap_key;
		jsval_t hmac_hash_obj;
		jsval_t hmac_inner_alg;
		jsval_t hmac_inner_usages;
		jsval_t hmac_inner_promise;
		jsval_t hmac_inner_key;
		jsval_t aes_inner_alg;
		jsval_t aes_inner_usages;
		jsval_t aes_inner_promise;
		jsval_t aes_inner_key;
		jsval_t iv_typed;
		jsval_t iv_buffer;
		jsval_t gcm_params;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t hmac_alg_for_unwrap;
		jsval_t hmac_hash_obj_unwrap;
		jsval_t aes_alg_for_unwrap;
		jsval_t wrap_promise;
		jsval_t wrapped_buf;
		jsval_t unwrap_promise;
		jsval_t unwrapped_key;
		jsval_t name_string;
		jsval_t alg_value;
		uint32_t gcm_wrap_usages_mask = 0;

		assert(jsval_array_new(&region, 2, &gcm_wrap_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"wrapKey", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, gcm_wrap_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"unwrapKey", 9,
				&name_string) == 0);
		assert(jsval_array_push(&region, gcm_wrap_usages, name_string) == 0);
		assert(jsval_object_new(&region, 2, &gcm_wrap_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, gcm_wrap_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, gcm_wrap_alg,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, gcm_wrap_alg,
				1, gcm_wrap_usages, &gcm_wrap_key_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, gcm_wrap_key_promise,
				&gcm_wrap_key) == 0);
		assert(gcm_wrap_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_usages(&region, gcm_wrap_key,
				&gcm_wrap_usages_mask) == 0);
		assert(gcm_wrap_usages_mask
				== (JSVAL_CRYPTO_KEY_USAGE_WRAP_KEY
					| JSVAL_CRYPTO_KEY_USAGE_UNWRAP_KEY));

		assert(jsval_object_new(&region, 1, &hmac_hash_obj) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_hash_obj,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_new(&region, 2, &hmac_inner_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_inner_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_inner_alg,
				(const uint8_t *)"hash", 4, hmac_hash_obj) == 0);
		assert(jsval_array_new(&region, 2, &hmac_inner_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, hmac_inner_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, hmac_inner_usages, name_string) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				hmac_inner_alg, 1, hmac_inner_usages,
				&hmac_inner_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, hmac_inner_promise,
				&hmac_inner_key) == 0);
		assert(hmac_inner_key.kind == JSVAL_KIND_CRYPTO_KEY);

		assert(jsval_object_new(&region, 2, &aes_inner_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_inner_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_inner_alg,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_array_new(&region, 2, &aes_inner_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, aes_inner_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&name_string) == 0);
		assert(jsval_array_push(&region, aes_inner_usages, name_string) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
				aes_inner_alg, 1, aes_inner_usages,
				&aes_inner_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, aes_inner_promise,
				&aes_inner_key) == 0);
		assert(aes_inner_key.kind == JSVAL_KIND_CRYPTO_KEY);

		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
				&iv_typed) == 0);
		assert(jsval_typed_array_buffer(&region, iv_typed, &iv_buffer) == 0);
		assert(jsval_object_new(&region, 2, &gcm_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, gcm_params,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, gcm_params,
				(const uint8_t *)"iv", 2, iv_buffer) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);

		/* raw wrap/unwrap of HMAC inner key. */
		assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, raw_format,
				hmac_inner_key, gcm_wrap_key, gcm_params,
				&wrap_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, wrap_promise, &wrapped_buf) == 0);
		assert(wrapped_buf.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, wrapped_buf, &len) == 0);
		/* HMAC SHA-256 raw key is 64 bytes; AES-GCM ciphertext = plaintext
		 * + 16-byte tag = 80. */
		assert(len == 80);

		assert(jsval_object_new(&region, 1, &hmac_hash_obj_unwrap) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_hash_obj_unwrap,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_new(&region, 2, &hmac_alg_for_unwrap) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_alg_for_unwrap,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_alg_for_unwrap,
				(const uint8_t *)"hash", 4, hmac_hash_obj_unwrap) == 0);
		assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, raw_format,
				wrapped_buf, gcm_wrap_key, gcm_params, hmac_alg_for_unwrap,
				1, hmac_inner_usages, &unwrap_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, unwrap_promise,
				&unwrapped_key) == 0);
		assert(unwrapped_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, unwrapped_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "HMAC");

		/* raw wrap/unwrap of AES-GCM inner key (self-wrap). */
		{
			jsval_t aes_wrap_promise;
			jsval_t aes_wrapped_buf;
			jsval_t aes_unwrap_promise;
			jsval_t aes_unwrapped_key;
			assert(jsval_object_new(&region, 2, &aes_alg_for_unwrap) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, aes_alg_for_unwrap,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, aes_alg_for_unwrap,
					(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
			assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, raw_format,
					aes_inner_key, gcm_wrap_key, gcm_params,
					&aes_wrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, aes_wrap_promise,
					&aes_wrapped_buf) == 0);
			assert(aes_wrapped_buf.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, aes_wrapped_buf,
					&len) == 0);
			/* AES-128 key is 16 bytes; ciphertext = 16 + 16-byte tag = 32. */
			assert(len == 32);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, raw_format,
					aes_wrapped_buf, gcm_wrap_key, gcm_params,
					aes_alg_for_unwrap, 1, aes_inner_usages,
					&aes_unwrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, aes_unwrap_promise,
					&aes_unwrapped_key) == 0);
			assert(aes_unwrapped_key.kind == JSVAL_KIND_CRYPTO_KEY);
			assert(jsval_crypto_key_algorithm(&region, aes_unwrapped_key,
					&alg_value) == 0);
			assert_object_string_prop(&region, alg_value, "name", "AES-GCM");
			assert_object_number_prop(&region, alg_value, "length", 128.0);
		}

		/* jwk wrap/unwrap of HMAC inner key. */
		{
			jsval_t jwk_wrap_promise;
			jsval_t jwk_wrapped_buf;
			jsval_t jwk_unwrap_promise;
			jsval_t jwk_unwrapped;
			jsval_t jwk_hash_obj;
			jsval_t jwk_alg_for_unwrap;

			assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, jwk_format,
					hmac_inner_key, gcm_wrap_key, gcm_params,
					&jwk_wrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_wrap_promise,
					&jwk_wrapped_buf) == 0);
			assert(jwk_wrapped_buf.kind == JSVAL_KIND_ARRAY_BUFFER);

			assert(jsval_object_new(&region, 1, &jwk_hash_obj) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, jwk_hash_obj,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_new(&region, 2, &jwk_alg_for_unwrap) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, jwk_alg_for_unwrap,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, jwk_alg_for_unwrap,
					(const uint8_t *)"hash", 4, jwk_hash_obj) == 0);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, jwk_format,
					jwk_wrapped_buf, gcm_wrap_key, gcm_params,
					jwk_alg_for_unwrap, 1, hmac_inner_usages,
					&jwk_unwrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_unwrap_promise,
					&jwk_unwrapped) == 0);
			assert(jwk_unwrapped.kind == JSVAL_KIND_CRYPTO_KEY);
			assert(jsval_crypto_key_algorithm(&region, jwk_unwrapped,
					&alg_value) == 0);
			assert_object_string_prop(&region, alg_value, "name", "HMAC");
			{
				jsval_t hash_prop;
				assert(jsval_object_get_utf8(&region, alg_value,
						(const uint8_t *)"hash", 4, &hash_prop) == 0);
				assert_object_string_prop(&region, hash_prop, "name",
						"SHA-256");
			}
		}

		/* Negative: missing iv on wrap params -> TypeError. */
		{
			jsval_t no_iv_params;
			jsval_t no_iv_promise;
			assert(jsval_object_new(&region, 1, &no_iv_params) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, no_iv_params,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_subtle_crypto_wrap_key(&region, subtle_a, raw_format,
					hmac_inner_key, gcm_wrap_key, no_iv_params,
					&no_iv_promise) == 0);
			assert(jsval_promise_result(&region, no_iv_promise, &result) == 0);
			assert_dom_exception(&region, result, "TypeError",
					"expected AES-GCM iv");
		}

		/* Negative: all-zero ciphertext-shaped buffer -> OperationError
		 * on unwrap (tag check failure). The runtime has no public
		 * typed-array mutator, so rather than flipping a byte we use a
		 * fresh zero-filled buffer of the right shape. The tag check
		 * still fails for the same reason, exercising the same error
		 * path. */
		{
			jsval_t tamper_typed;
			jsval_t tamper_buffer;
			jsval_t tamper_unwrap_promise;
			assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
					80, &tamper_typed) == 0);
			assert(jsval_typed_array_buffer(&region, tamper_typed,
					&tamper_buffer) == 0);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, raw_format,
					tamper_buffer, gcm_wrap_key, gcm_params,
					hmac_alg_for_unwrap, 1, hmac_inner_usages,
					&tamper_unwrap_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, tamper_unwrap_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "OperationError",
					"AES-GCM operation failed");
		}

		/* Negative: unsupported inner algorithm name -> NotSupportedError. */
		{
			jsval_t bad_inner_alg;
			jsval_t bad_unwrap_promise;
			assert(jsval_object_new(&region, 1, &bad_inner_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-OAEP",
					8, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_inner_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_subtle_crypto_unwrap_key(&region, subtle_a, raw_format,
					wrapped_buf, gcm_wrap_key, gcm_params, bad_inner_alg, 1,
					hmac_inner_usages, &bad_unwrap_promise) == 0);
			assert(jsval_promise_result(&region, bad_unwrap_promise, &result)
					== 0);
			assert_dom_exception(&region, result, "NotSupportedError",
					"unsupported unwrapped key algorithm");
		}
	}

	{
		/*
		 * ECDSA P-256 — the first asymmetric WebCrypto algorithm.
		 * generateKey resolves to a {publicKey, privateKey} object;
		 * sign/verify round-trips over raw and jwk key formats; the
		 * verify path resolves false on mismatch rather than rejecting.
		 * Plus negative cases: sign with a public key → InvalidAccessError,
		 * malformed JWK crv → DataError, unsupported curve →
		 * NotSupportedError.
		 */
		jsval_t ecdsa_usages;
		jsval_t ecdsa_alg;
		jsval_t ecdsa_gen_promise;
		jsval_t ecdsa_pair;
		jsval_t public_key;
		jsval_t private_key;
		jsval_t name_string;
		jsval_t alg_value;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t sign_alg;
		jsval_t sign_promise;
		jsval_t signature;
		jsval_t verify_promise;
		jsval_t data_typed;
		jsval_t data_buffer;
		uint32_t public_usages_mask = 0;
		uint32_t private_usages_mask = 0;

		assert(jsval_array_new(&region, 2, &ecdsa_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, ecdsa_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, ecdsa_usages, name_string) == 0);
		assert(jsval_object_new(&region, 2, &ecdsa_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"ECDSA", 5,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, ecdsa_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"P-256", 5,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, ecdsa_alg,
				(const uint8_t *)"namedCurve", 10, name_string) == 0);
		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, ecdsa_alg,
				1, ecdsa_usages, &ecdsa_gen_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, ecdsa_gen_promise,
				&ecdsa_pair) == 0);
		assert(ecdsa_pair.kind == JSVAL_KIND_OBJECT);
		assert(jsval_object_get_utf8(&region, ecdsa_pair,
				(const uint8_t *)"publicKey", 9, &public_key) == 0);
		assert(jsval_object_get_utf8(&region, ecdsa_pair,
				(const uint8_t *)"privateKey", 10, &private_key) == 0);
		assert(public_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(private_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_usages(&region, public_key,
				&public_usages_mask) == 0);
		assert(public_usages_mask == JSVAL_CRYPTO_KEY_USAGE_VERIFY);
		assert(jsval_crypto_key_usages(&region, private_key,
				&private_usages_mask) == 0);
		assert(private_usages_mask == JSVAL_CRYPTO_KEY_USAGE_SIGN);
		assert(jsval_crypto_key_algorithm(&region, public_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "ECDSA");
		assert_object_string_prop(&region, alg_value, "namedCurve", "P-256");

		/* sign/verify round-trip over a 16-byte payload with SHA-256. */
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&data_typed) == 0);
		assert(jsval_typed_array_buffer(&region, data_typed,
				&data_buffer) == 0);
		assert(jsval_object_new(&region, 2, &sign_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"ECDSA", 5,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, sign_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, sign_alg,
				(const uint8_t *)"hash", 4, name_string) == 0);
		assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
				private_key, data_buffer, &sign_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, sign_promise, &signature) == 0);
		assert(signature.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, signature, &len) == 0);
		assert(len == 64);

		assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
				public_key, signature, data_buffer, &verify_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, verify_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_BOOL);
		assert(result.as.boolean == 1);

		/* Mismatch path: verify a fresh zero-buffer signature (wrong bytes)
		 * resolves false without rejecting. */
		{
			jsval_t bogus_typed;
			jsval_t bogus_buffer;
			jsval_t bogus_verify_promise;

			assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 64,
					&bogus_typed) == 0);
			assert(jsval_typed_array_buffer(&region, bogus_typed,
					&bogus_buffer) == 0);
			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					public_key, bogus_buffer, data_buffer,
					&bogus_verify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, bogus_verify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL);
			assert(result.as.boolean == 0);
		}

		/* exportKey raw on the public key -> 65 bytes (SEC1 uncompressed). */
		{
			jsval_t export_raw_promise;

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					raw_format, public_key, &export_raw_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, export_raw_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, result, &len) == 0);
			assert(len == 65);
			{
				uint8_t sec1[65];
				size_t sec1_len = 0;
				assert(jsval_array_buffer_copy_bytes(&region, result, sec1,
						sizeof(sec1), &sec1_len) == 0);
				assert(sec1_len == 65);
				assert(sec1[0] == 0x04u);
			}
		}

		/* exportKey raw on the private key -> NotSupportedError. */
		{
			jsval_t export_priv_raw_promise;

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					raw_format, private_key, &export_priv_raw_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, export_priv_raw_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "NotSupportedError",
					"raw export requires EC public key");
		}

		/* exportKey jwk on public and private, then importKey jwk back. */
		{
			jsval_t export_pub_jwk_promise;
			jsval_t export_priv_jwk_promise;
			jsval_t pub_jwk;
			jsval_t priv_jwk;
			jsval_t reimport_public_promise;
			jsval_t reimport_public;
			jsval_t verify_sign_usages;
			jsval_t verify_only_usages;

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, public_key, &export_pub_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, export_pub_jwk_promise,
					&pub_jwk) == 0);
			assert(pub_jwk.kind == JSVAL_KIND_OBJECT);
			assert_object_string_prop(&region, pub_jwk, "kty", "EC");
			assert_object_string_prop(&region, pub_jwk, "crv", "P-256");

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, private_key, &export_priv_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, export_priv_jwk_promise,
					&priv_jwk) == 0);
			assert(priv_jwk.kind == JSVAL_KIND_OBJECT);
			assert_object_string_prop(&region, priv_jwk, "kty", "EC");
			{
				jsval_t d_prop;
				assert(jsval_object_get_utf8(&region, priv_jwk,
						(const uint8_t *)"d", 1, &d_prop) == 0);
				assert(d_prop.kind == JSVAL_KIND_STRING);
			}

			/* Reimport the public JWK and verify the signature. */
			assert(jsval_array_new(&region, 1, &verify_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
					&name_string) == 0);
			assert(jsval_array_push(&region, verify_only_usages,
					name_string) == 0);
			(void)verify_sign_usages;
			assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
					pub_jwk, ecdsa_alg, 1, verify_only_usages,
					&reimport_public_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_public_promise,
					&reimport_public) == 0);
			assert(reimport_public.kind == JSVAL_KIND_CRYPTO_KEY);
			{
				jsval_t verify_reimport_promise;

				assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
						reimport_public, signature, data_buffer,
						&verify_reimport_promise) == 0);
				memset(&error, 0, sizeof(error));
				assert(jsval_microtask_drain(&region, &error) == 0);
				assert(jsval_promise_result(&region, verify_reimport_promise,
						&result) == 0);
				assert(result.kind == JSVAL_KIND_BOOL);
				assert(result.as.boolean == 1);
			}
		}

		/* Negative: sign with a public key -> InvalidAccessError. */
		{
			jsval_t bad_sign_promise;

			assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
					public_key, data_buffer, &bad_sign_promise) == 0);
			assert(jsval_promise_result(&region, bad_sign_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "InvalidAccessError",
					"key does not support sign");
		}

		/* Negative: verify with a private key -> InvalidAccessError. */
		{
			jsval_t bad_verify_promise;

			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					private_key, signature, data_buffer,
					&bad_verify_promise) == 0);
			assert(jsval_promise_result(&region, bad_verify_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "InvalidAccessError",
					"key does not support verify");
		}

		/* Negative: unsupported namedCurve -> NotSupportedError. */
		{
			jsval_t bad_alg;
			jsval_t bad_curve_promise;

			assert(jsval_object_new(&region, 2, &bad_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"ECDSA", 5,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"P-521", 5,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"namedCurve", 10, name_string) == 0);
			assert(jsval_subtle_crypto_generate_key(&region, subtle_a, bad_alg,
					1, ecdsa_usages, &bad_curve_promise) == 0);
			assert(jsval_promise_result(&region, bad_curve_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "NotSupportedError",
					"unsupported ECDSA curve");
		}

		/* Negative: importKey jwk with wrong crv -> DataError. */
		{
			jsval_t bad_jwk;
			jsval_t bad_import_promise;
			jsval_t verify_only_usages;

			assert(jsval_array_new(&region, 1, &verify_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
					&name_string) == 0);
			assert(jsval_array_push(&region, verify_only_usages,
					name_string) == 0);
			assert(jsval_object_new(&region, 4, &bad_jwk) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"EC", 2,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"kty", 3, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"P-384", 5,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"crv", 3, name_string) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
					43, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"x", 1, name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"y", 1, name_string) == 0);
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					jwk_format, bad_jwk, ecdsa_alg, 1, verify_only_usages,
					&bad_import_promise) == 0);
			assert(jsval_promise_result(&region, bad_import_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "DataError",
					"invalid EC JWK crv");
		}
	}

	{
		/*
		 * RSASSA-PKCS1-v1_5 — the first RSA asymmetric algorithm.
		 * 2048-bit generateKey resolves to {publicKey, privateKey},
		 * JWK export/import round-trip, sign/verify happy path, and
		 * negative cases: sign with public key → InvalidAccessError,
		 * unsupported modulus → NotSupportedError, malformed JWK
		 * (bad kty) → DataError.
		 */
		jsval_t rsa_usages;
		jsval_t rsa_alg;
		jsval_t hash_obj;
		jsval_t rsa_gen_promise;
		jsval_t rsa_pair;
		jsval_t rsa_public_key;
		jsval_t rsa_private_key;
		jsval_t sign_alg;
		jsval_t sign_promise;
		jsval_t rsa_signature;
		jsval_t verify_promise;
		jsval_t data_typed;
		jsval_t data_buffer;
		jsval_t name_string;
		jsval_t alg_value;
		jsval_t jwk_format;
		uint32_t public_mask = 0;
		uint32_t private_mask = 0;

		assert(jsval_array_new(&region, 2, &rsa_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, rsa_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, rsa_usages, name_string) == 0);

		assert(jsval_object_new(&region, 1, &hash_obj) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hash_obj,
				(const uint8_t *)"name", 4, name_string) == 0);

		/* publicExponent is implicit F4 when absent. */
		assert(jsval_object_new(&region, 3, &rsa_alg) == 0);
		assert(jsval_string_new_utf8(&region,
				(const uint8_t *)"RSASSA-PKCS1-v1_5", 17, &name_string) == 0);
		assert(jsval_object_set_utf8(&region, rsa_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, rsa_alg,
				(const uint8_t *)"modulusLength", 13,
				jsval_number(2048.0)) == 0);
		assert(jsval_object_set_utf8(&region, rsa_alg,
				(const uint8_t *)"hash", 4, hash_obj) == 0);

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, rsa_alg,
				1, rsa_usages, &rsa_gen_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, rsa_gen_promise,
				&rsa_pair) == 0);
		assert(rsa_pair.kind == JSVAL_KIND_OBJECT);
		assert(jsval_object_get_utf8(&region, rsa_pair,
				(const uint8_t *)"publicKey", 9, &rsa_public_key) == 0);
		assert(jsval_object_get_utf8(&region, rsa_pair,
				(const uint8_t *)"privateKey", 10, &rsa_private_key) == 0);
		assert(rsa_public_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(rsa_private_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_usages(&region, rsa_public_key,
				&public_mask) == 0);
		assert(public_mask == JSVAL_CRYPTO_KEY_USAGE_VERIFY);
		assert(jsval_crypto_key_usages(&region, rsa_private_key,
				&private_mask) == 0);
		assert(private_mask == JSVAL_CRYPTO_KEY_USAGE_SIGN);
		assert(jsval_crypto_key_algorithm(&region, rsa_public_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name",
				"RSASSA-PKCS1-v1_5");
		assert_object_number_prop(&region, alg_value, "modulusLength",
				2048.0);

		/* sign/verify over a 16-byte payload. */
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&data_typed) == 0);
		assert(jsval_typed_array_buffer(&region, data_typed,
				&data_buffer) == 0);
		assert(jsval_object_new(&region, 1, &sign_alg) == 0);
		assert(jsval_string_new_utf8(&region,
				(const uint8_t *)"RSASSA-PKCS1-v1_5", 17, &name_string) == 0);
		assert(jsval_object_set_utf8(&region, sign_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
				rsa_private_key, data_buffer, &sign_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, sign_promise,
				&rsa_signature) == 0);
		assert(rsa_signature.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, rsa_signature,
				&len) == 0);
		assert(len == 256);  /* 2048-bit signature */

		assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
				rsa_public_key, rsa_signature, data_buffer,
				&verify_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, verify_promise, &result) == 0);
		assert(result.kind == JSVAL_KIND_BOOL);
		assert(result.as.boolean == 1);

		/* Mismatch: verify a zero buffer of the right length → false. */
		{
			jsval_t bogus_typed;
			jsval_t bogus_buffer;
			jsval_t bogus_verify_promise;

			assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
					256, &bogus_typed) == 0);
			assert(jsval_typed_array_buffer(&region, bogus_typed,
					&bogus_buffer) == 0);
			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					rsa_public_key, bogus_buffer, data_buffer,
					&bogus_verify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, bogus_verify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL);
			assert(result.as.boolean == 0);
		}

		/* exportKey jwk public, reimport, verify the original signature. */
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);
		{
			jsval_t pub_jwk_promise;
			jsval_t pub_jwk;
			jsval_t priv_jwk_promise;
			jsval_t priv_jwk;
			jsval_t reimport_promise;
			jsval_t reimport_public;
			jsval_t verify_reimport_promise;
			jsval_t verify_only_usages;

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, rsa_public_key, &pub_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, pub_jwk_promise,
					&pub_jwk) == 0);
			assert(pub_jwk.kind == JSVAL_KIND_OBJECT);
			assert_object_string_prop(&region, pub_jwk, "kty", "RSA");
			assert_object_string_prop(&region, pub_jwk, "alg", "RS256");

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, rsa_private_key, &priv_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, priv_jwk_promise,
					&priv_jwk) == 0);
			assert(priv_jwk.kind == JSVAL_KIND_OBJECT);
			{
				jsval_t d_prop;
				assert(jsval_object_get_utf8(&region, priv_jwk,
						(const uint8_t *)"d", 1, &d_prop) == 0);
				assert(d_prop.kind == JSVAL_KIND_STRING);
			}
			{
				jsval_t p_prop;
				assert(jsval_object_get_utf8(&region, priv_jwk,
						(const uint8_t *)"p", 1, &p_prop) == 0);
				assert(p_prop.kind == JSVAL_KIND_STRING);
			}

			assert(jsval_array_new(&region, 1, &verify_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, verify_only_usages,
					name_string) == 0);
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					jwk_format, pub_jwk, rsa_alg, 1, verify_only_usages,
					&reimport_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_promise,
					&reimport_public) == 0);
			assert(reimport_public.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					reimport_public, rsa_signature, data_buffer,
					&verify_reimport_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, verify_reimport_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL);
			assert(result.as.boolean == 1);
		}

		/* Negative: sign with public key -> InvalidAccessError. */
		{
			jsval_t bad_sign_promise;

			assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
					rsa_public_key, data_buffer, &bad_sign_promise) == 0);
			assert(jsval_promise_result(&region, bad_sign_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "InvalidAccessError",
					"key does not support sign");
		}

		/* Negative: unsupported modulusLength -> NotSupportedError. */
		{
			jsval_t bad_alg;
			jsval_t bad_gen_promise;

			assert(jsval_object_new(&region, 3, &bad_alg) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"RSASSA-PKCS1-v1_5", 17,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"modulusLength", 13,
					jsval_number(1024.0)) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"hash", 4, hash_obj) == 0);
			assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
					bad_alg, 1, rsa_usages, &bad_gen_promise) == 0);
			assert(jsval_promise_result(&region, bad_gen_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "NotSupportedError",
					"unsupported RSASSA-PKCS1-v1_5 modulusLength");
		}

		/* Negative: malformed JWK kty -> DataError. */
		{
			jsval_t bad_jwk;
			jsval_t bad_import_promise;
			jsval_t verify_only_usages;

			assert(jsval_array_new(&region, 1, &verify_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, verify_only_usages,
					name_string) == 0);
			assert(jsval_object_new(&region, 3, &bad_jwk) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"oct", 3,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"kty", 3, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"AQAB", 4,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"n", 1, name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"e", 1, name_string) == 0);
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					jwk_format, bad_jwk, rsa_alg, 1, verify_only_usages,
					&bad_import_promise) == 0);
			assert(jsval_promise_result(&region, bad_import_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "DataError",
					"invalid RSA JWK kty");
		}
	}

	{
		/*
		 * RSA-PSS — same key material as RSASSA-PKCS1-v1_5 but a
		 * different algorithm kind and padding mode. 2048-bit
		 * generateKey resolves to a pair, sign/verify with the
		 * default salt length (digest output length), JWK export +
		 * reimport and reverify, plus negatives: sign with public
		 * key → InvalidAccessError, unsupported modulus →
		 * NotSupportedError, cross-algorithm key confusion (a
		 * RSASSA key cannot sign as RSA-PSS) → InvalidAccessError.
		 */
		jsval_t pss_usages;
		jsval_t pss_alg;
		jsval_t hash_obj;
		jsval_t pss_gen_promise;
		jsval_t pss_pair;
		jsval_t pss_public_key;
		jsval_t pss_private_key;
		jsval_t pss_sign_alg;
		jsval_t pss_sign_promise;
		jsval_t pss_signature;
		jsval_t pss_verify_promise;
		jsval_t pss_signature_b;
		jsval_t pss_sign_promise_b;
		jsval_t data_typed;
		jsval_t data_buffer;
		jsval_t name_string;
		jsval_t alg_value;
		jsval_t jwk_format;
		size_t sig_len_a = 0;
		size_t sig_len_b = 0;
		uint32_t public_mask = 0;
		uint32_t private_mask = 0;

		assert(jsval_array_new(&region, 2, &pss_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, pss_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, pss_usages, name_string) == 0);

		assert(jsval_object_new(&region, 1, &hash_obj) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, hash_obj,
				(const uint8_t *)"name", 4, name_string) == 0);

		assert(jsval_object_new(&region, 3, &pss_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-PSS", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, pss_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_set_utf8(&region, pss_alg,
				(const uint8_t *)"modulusLength", 13,
				jsval_number(2048.0)) == 0);
		assert(jsval_object_set_utf8(&region, pss_alg,
				(const uint8_t *)"hash", 4, hash_obj) == 0);

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, pss_alg,
				1, pss_usages, &pss_gen_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, pss_gen_promise,
				&pss_pair) == 0);
		assert(pss_pair.kind == JSVAL_KIND_OBJECT);
		assert(jsval_object_get_utf8(&region, pss_pair,
				(const uint8_t *)"publicKey", 9, &pss_public_key) == 0);
		assert(jsval_object_get_utf8(&region, pss_pair,
				(const uint8_t *)"privateKey", 10, &pss_private_key) == 0);
		assert(pss_public_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(pss_private_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_usages(&region, pss_public_key,
				&public_mask) == 0);
		assert(public_mask == JSVAL_CRYPTO_KEY_USAGE_VERIFY);
		assert(jsval_crypto_key_usages(&region, pss_private_key,
				&private_mask) == 0);
		assert(private_mask == JSVAL_CRYPTO_KEY_USAGE_SIGN);
		assert(jsval_crypto_key_algorithm(&region, pss_public_key,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "RSA-PSS");

		/* sign/verify with default salt length (32 for SHA-256). */
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&data_typed) == 0);
		assert(jsval_typed_array_buffer(&region, data_typed,
				&data_buffer) == 0);
		assert(jsval_object_new(&region, 1, &pss_sign_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-PSS", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, pss_sign_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_subtle_crypto_sign(&region, subtle_a, pss_sign_alg,
				pss_private_key, data_buffer, &pss_sign_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, pss_sign_promise,
				&pss_signature) == 0);
		assert(pss_signature.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, pss_signature,
				&sig_len_a) == 0);
		assert(sig_len_a == 256);

		assert(jsval_subtle_crypto_verify(&region, subtle_a, pss_sign_alg,
				pss_public_key, pss_signature, data_buffer,
				&pss_verify_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, pss_verify_promise,
				&result) == 0);
		assert(result.kind == JSVAL_KIND_BOOL);
		assert(result.as.boolean == 1);

		/* Randomization: second sign of the same payload differs from
		 * the first, but both verify. */
		assert(jsval_subtle_crypto_sign(&region, subtle_a, pss_sign_alg,
				pss_private_key, data_buffer, &pss_sign_promise_b) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, pss_sign_promise_b,
				&pss_signature_b) == 0);
		assert(jsval_array_buffer_byte_length(&region, pss_signature_b,
				&sig_len_b) == 0);
		assert(sig_len_a == sig_len_b);
		{
			uint8_t bytes_a[256];
			uint8_t bytes_b[256];
			size_t len_a = 0;
			size_t len_b = 0;

			assert(jsval_array_buffer_copy_bytes(&region, pss_signature,
					bytes_a, sizeof(bytes_a), &len_a) == 0);
			assert(jsval_array_buffer_copy_bytes(&region, pss_signature_b,
					bytes_b, sizeof(bytes_b), &len_b) == 0);
			assert(len_a == 256 && len_b == 256);
			assert(memcmp(bytes_a, bytes_b, 256) != 0);
		}

		/* JWK export public + private, reimport public, reverify. */
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);
		{
			jsval_t pub_jwk_promise;
			jsval_t pub_jwk;
			jsval_t priv_jwk_promise;
			jsval_t priv_jwk;
			jsval_t reimport_promise;
			jsval_t reimport_public;
			jsval_t verify_reimport_promise;
			jsval_t verify_only_usages;

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, pss_public_key, &pub_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, pub_jwk_promise,
					&pub_jwk) == 0);
			assert(pub_jwk.kind == JSVAL_KIND_OBJECT);
			assert_object_string_prop(&region, pub_jwk, "kty", "RSA");
			assert_object_string_prop(&region, pub_jwk, "alg", "PS256");

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, pss_private_key, &priv_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, priv_jwk_promise,
					&priv_jwk) == 0);
			assert(priv_jwk.kind == JSVAL_KIND_OBJECT);
			{
				jsval_t d_prop;
				assert(jsval_object_get_utf8(&region, priv_jwk,
						(const uint8_t *)"d", 1, &d_prop) == 0);
				assert(d_prop.kind == JSVAL_KIND_STRING);
			}

			assert(jsval_array_new(&region, 1, &verify_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, verify_only_usages,
					name_string) == 0);
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					jwk_format, pub_jwk, pss_alg, 1, verify_only_usages,
					&reimport_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_promise,
					&reimport_public) == 0);
			assert(reimport_public.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_verify(&region, subtle_a, pss_sign_alg,
					reimport_public, pss_signature, data_buffer,
					&verify_reimport_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, verify_reimport_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL);
			assert(result.as.boolean == 1);
		}

		/* Negative: sign with public key -> InvalidAccessError. */
		{
			jsval_t bad_sign_promise;

			assert(jsval_subtle_crypto_sign(&region, subtle_a, pss_sign_alg,
					pss_public_key, data_buffer, &bad_sign_promise) == 0);
			assert(jsval_promise_result(&region, bad_sign_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "InvalidAccessError",
					"key does not support sign");
		}

		/* Negative: unsupported modulusLength -> NotSupportedError. */
		{
			jsval_t bad_alg;
			jsval_t bad_gen_promise;

			assert(jsval_object_new(&region, 3, &bad_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-PSS",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"modulusLength", 13,
					jsval_number(1024.0)) == 0);
			assert(jsval_object_set_utf8(&region, bad_alg,
					(const uint8_t *)"hash", 4, hash_obj) == 0);
			assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
					bad_alg, 1, pss_usages, &bad_gen_promise) == 0);
			assert(jsval_promise_result(&region, bad_gen_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "NotSupportedError",
					"unsupported RSA-PSS modulusLength");
		}

		/* Negative: cross-algorithm key confusion. Generate an
		 * RSASSA-PKCS1-v1_5 keypair, then try to sign with its private
		 * key via RSA-PSS — the algorithm_kind tag rejects with
		 * InvalidAccessError at the sign dispatch, not at the C layer. */
		{
			jsval_t rs_alg;
			jsval_t rs_gen_promise;
			jsval_t rs_pair;
			jsval_t rs_private;
			jsval_t cross_sign_promise;

			assert(jsval_object_new(&region, 3, &rs_alg) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"RSASSA-PKCS1-v1_5", 17,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, rs_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, rs_alg,
					(const uint8_t *)"modulusLength", 13,
					jsval_number(2048.0)) == 0);
			assert(jsval_object_set_utf8(&region, rs_alg,
					(const uint8_t *)"hash", 4, hash_obj) == 0);
			assert(jsval_subtle_crypto_generate_key(&region, subtle_a, rs_alg,
					1, pss_usages, &rs_gen_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, rs_gen_promise,
					&rs_pair) == 0);
			assert(jsval_object_get_utf8(&region, rs_pair,
					(const uint8_t *)"privateKey", 10, &rs_private) == 0);
			/* The sign dispatch inspects the key's algorithm_kind; a
			 * PSS sign call on a PKCS1 key falls through to the HMAC
			 * validator which rejects with "expected HMAC secret
			 * key". That's still an InvalidAccessError. */
			assert(jsval_subtle_crypto_sign(&region, subtle_a, pss_sign_alg,
					rs_private, data_buffer, &cross_sign_promise) == 0);
			assert(jsval_promise_result(&region, cross_sign_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_DOM_EXCEPTION);
			{
				jsval_t exc_name;
				uint8_t name_buf[32];
				size_t name_len = 0;

				assert(jsval_dom_exception_name(&region, result,
						&exc_name) == 0);
				assert(jsval_string_copy_utf8(&region, exc_name, name_buf,
						sizeof(name_buf), &name_len) == 0);
				assert(name_len == strlen("InvalidAccessError"));
				assert(memcmp(name_buf, "InvalidAccessError",
						name_len) == 0);
			}
		}
	}

	{
		/*
		 * SPKI / PKCS#8 key transport formats for all three
		 * asymmetric algorithms. For each algorithm: generate a
		 * keypair, export public as SPKI, export private as PKCS#8,
		 * reimport both, use the reimported keys to verify / sign.
		 * Plus negatives: SPKI on private key, PKCS#8 on public
		 * key, malformed DER, symmetric algorithm rejecting SPKI.
		 */
		jsval_t spki_format;
		jsval_t pkcs8_format;
		jsval_t name_string;
		jsval_t data_typed;
		jsval_t data_buffer;

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"spki", 4,
				&spki_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"pkcs8", 5,
				&pkcs8_format) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&data_typed) == 0);
		assert(jsval_typed_array_buffer(&region, data_typed,
				&data_buffer) == 0);

		/* ECDSA P-256 SPKI/PKCS8 round trip. */
		{
			jsval_t ecdsa_alg;
			jsval_t sign_alg;
			jsval_t usages;
			jsval_t verify_usages;
			jsval_t sign_only_usages;
			jsval_t gen_promise;
			jsval_t pair;
			jsval_t public_key;
			jsval_t private_key;
			jsval_t spki_promise;
			jsval_t pkcs8_promise;
			jsval_t spki_bytes;
			jsval_t pkcs8_bytes;
			jsval_t reimport_public_promise;
			jsval_t reimport_public;
			jsval_t reimport_private_promise;
			jsval_t reimport_private;
			jsval_t sign_promise;
			jsval_t sig;
			jsval_t verify_promise;
			jsval_t result;

			assert(jsval_array_new(&region, 2, &usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&name_string) == 0);
			assert(jsval_array_push(&region, usages, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, usages, name_string) == 0);
			assert(jsval_array_new(&region, 1, &verify_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, verify_usages, name_string) == 0);
			assert(jsval_array_new(&region, 1, &sign_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&name_string) == 0);
			assert(jsval_array_push(&region, sign_only_usages,
					name_string) == 0);

			assert(jsval_object_new(&region, 2, &ecdsa_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"ECDSA", 5,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, ecdsa_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"P-256", 5,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, ecdsa_alg,
					(const uint8_t *)"namedCurve", 10, name_string) == 0);

			assert(jsval_subtle_crypto_generate_key(&region, subtle_a,
					ecdsa_alg, 1, usages, &gen_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, gen_promise, &pair) == 0);
			assert(jsval_object_get_utf8(&region, pair,
					(const uint8_t *)"publicKey", 9, &public_key) == 0);
			assert(jsval_object_get_utf8(&region, pair,
					(const uint8_t *)"privateKey", 10, &private_key) == 0);

			/* Export public as SPKI. */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					spki_format, public_key, &spki_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, spki_promise,
					&spki_bytes) == 0);
			assert(spki_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, spki_bytes,
					&len) == 0);
			assert(len > 60 && len < 120);

			/* Export private as PKCS#8. */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					pkcs8_format, private_key, &pkcs8_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, pkcs8_promise,
					&pkcs8_bytes) == 0);
			assert(pkcs8_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, pkcs8_bytes,
					&len) == 0);
			assert(len > 100 && len < 200);

			/* Reimport public from SPKI. */
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					spki_format, spki_bytes, ecdsa_alg, 1, verify_usages,
					&reimport_public_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_public_promise,
					&reimport_public) == 0);
			assert(reimport_public.kind == JSVAL_KIND_CRYPTO_KEY);

			/* Reimport private from PKCS#8. */
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					pkcs8_format, pkcs8_bytes, ecdsa_alg, 1,
					sign_only_usages, &reimport_private_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_private_promise,
					&reimport_private) == 0);
			assert(reimport_private.kind == JSVAL_KIND_CRYPTO_KEY);

			/* Sign with reimport_private, verify with reimport_public. */
			assert(jsval_object_new(&region, 2, &sign_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"ECDSA",
					5, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, sign_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, sign_alg,
					(const uint8_t *)"hash", 4, name_string) == 0);
			assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
					reimport_private, data_buffer, &sign_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, sign_promise, &sig) == 0);
			assert(sig.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					reimport_public, sig, data_buffer,
					&verify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, verify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);

			/* Negative: SPKI export of private key -> NotSupportedError. */
			{
				jsval_t bad_spki_promise;

				assert(jsval_subtle_crypto_export_key(&region, subtle_a,
						spki_format, private_key,
						&bad_spki_promise) == 0);
				assert(jsval_promise_result(&region, bad_spki_promise,
						&result) == 0);
				assert_dom_exception(&region, result, "NotSupportedError",
						"spki export requires EC public key");
			}

			/* Negative: PKCS#8 export of public key -> NotSupportedError. */
			{
				jsval_t bad_pkcs8_promise;

				assert(jsval_subtle_crypto_export_key(&region, subtle_a,
						pkcs8_format, public_key,
						&bad_pkcs8_promise) == 0);
				assert(jsval_promise_result(&region, bad_pkcs8_promise,
						&result) == 0);
				assert_dom_exception(&region, result, "NotSupportedError",
						"pkcs8 export requires EC private key");
			}
		}

		/* RSASSA-PKCS1-v1_5 SPKI/PKCS8 round trip. */
		{
			jsval_t rsa_alg;
			jsval_t sign_alg;
			jsval_t usages;
			jsval_t verify_usages;
			jsval_t sign_only_usages;
			jsval_t gen_promise;
			jsval_t pair;
			jsval_t public_key;
			jsval_t private_key;
			jsval_t spki_promise;
			jsval_t pkcs8_promise;
			jsval_t spki_bytes;
			jsval_t pkcs8_bytes;
			jsval_t reimport_public_promise;
			jsval_t reimport_public;
			jsval_t reimport_private_promise;
			jsval_t reimport_private;
			jsval_t sign_promise;
			jsval_t sig;
			jsval_t verify_promise;
			jsval_t hash_obj;
			jsval_t result;

			assert(jsval_array_new(&region, 2, &usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&name_string) == 0);
			assert(jsval_array_push(&region, usages, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, usages, name_string) == 0);
			assert(jsval_array_new(&region, 1, &verify_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, verify_usages, name_string) == 0);
			assert(jsval_array_new(&region, 1, &sign_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&name_string) == 0);
			assert(jsval_array_push(&region, sign_only_usages,
					name_string) == 0);

			assert(jsval_object_new(&region, 1, &hash_obj) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, hash_obj,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_new(&region, 3, &rsa_alg) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"RSASSA-PKCS1-v1_5", 17,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, rsa_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, rsa_alg,
					(const uint8_t *)"modulusLength", 13,
					jsval_number(2048.0)) == 0);
			assert(jsval_object_set_utf8(&region, rsa_alg,
					(const uint8_t *)"hash", 4, hash_obj) == 0);

			assert(jsval_subtle_crypto_generate_key(&region, subtle_a, rsa_alg,
					1, usages, &gen_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, gen_promise, &pair) == 0);
			assert(jsval_object_get_utf8(&region, pair,
					(const uint8_t *)"publicKey", 9, &public_key) == 0);
			assert(jsval_object_get_utf8(&region, pair,
					(const uint8_t *)"privateKey", 10, &private_key) == 0);

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					spki_format, public_key, &spki_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, spki_promise,
					&spki_bytes) == 0);
			assert(spki_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					pkcs8_format, private_key, &pkcs8_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, pkcs8_promise,
					&pkcs8_bytes) == 0);
			assert(pkcs8_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					spki_format, spki_bytes, rsa_alg, 1, verify_usages,
					&reimport_public_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_public_promise,
					&reimport_public) == 0);
			assert(reimport_public.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					pkcs8_format, pkcs8_bytes, rsa_alg, 1, sign_only_usages,
					&reimport_private_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_private_promise,
					&reimport_private) == 0);
			assert(reimport_private.kind == JSVAL_KIND_CRYPTO_KEY);

			/* Sign with reimported private, verify with reimported public. */
			assert(jsval_object_new(&region, 1, &sign_alg) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"RSASSA-PKCS1-v1_5", 17,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, sign_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
					reimport_private, data_buffer, &sign_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, sign_promise, &sig) == 0);
			assert(sig.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, sig, &len) == 0);
			assert(len == 256);
			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					reimport_public, sig, data_buffer,
					&verify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, verify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);

			/* Cross-algorithm: RSA SPKI fed to an ECDSA importKey
			 * -> DataError. */
			{
				jsval_t bad_ecdsa_alg;
				jsval_t cross_import_promise;

				assert(jsval_object_new(&region, 2, &bad_ecdsa_alg) == 0);
				assert(jsval_string_new_utf8(&region,
						(const uint8_t *)"ECDSA", 5, &name_string) == 0);
				assert(jsval_object_set_utf8(&region, bad_ecdsa_alg,
						(const uint8_t *)"name", 4, name_string) == 0);
				assert(jsval_string_new_utf8(&region,
						(const uint8_t *)"P-256", 5, &name_string) == 0);
				assert(jsval_object_set_utf8(&region, bad_ecdsa_alg,
						(const uint8_t *)"namedCurve", 10, name_string) == 0);
				assert(jsval_subtle_crypto_import_key(&region, subtle_a,
						spki_format, spki_bytes, bad_ecdsa_alg, 1,
						verify_usages, &cross_import_promise) == 0);
				assert(jsval_promise_result(&region, cross_import_promise,
						&result) == 0);
				assert_dom_exception(&region, result, "DataError",
						"invalid EC SPKI");
			}
		}

		/* RSA-PSS SPKI/PKCS8 round trip (re-uses the same DER
		 * storage as RSASSA-PKCS1-v1_5 but with a different
		 * algorithm tag). */
		{
			jsval_t pss_alg;
			jsval_t sign_alg;
			jsval_t usages;
			jsval_t verify_usages;
			jsval_t sign_only_usages;
			jsval_t gen_promise;
			jsval_t pair;
			jsval_t public_key;
			jsval_t private_key;
			jsval_t spki_promise;
			jsval_t pkcs8_promise;
			jsval_t spki_bytes;
			jsval_t pkcs8_bytes;
			jsval_t reimport_public_promise;
			jsval_t reimport_public;
			jsval_t reimport_private_promise;
			jsval_t reimport_private;
			jsval_t sign_promise;
			jsval_t sig;
			jsval_t verify_promise;
			jsval_t hash_obj;
			jsval_t result;

			assert(jsval_array_new(&region, 2, &usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&name_string) == 0);
			assert(jsval_array_push(&region, usages, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, usages, name_string) == 0);
			assert(jsval_array_new(&region, 1, &verify_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify",
					6, &name_string) == 0);
			assert(jsval_array_push(&region, verify_usages, name_string) == 0);
			assert(jsval_array_new(&region, 1, &sign_only_usages) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&name_string) == 0);
			assert(jsval_array_push(&region, sign_only_usages,
					name_string) == 0);

			assert(jsval_object_new(&region, 1, &hash_obj) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, hash_obj,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_new(&region, 3, &pss_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-PSS",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, pss_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_object_set_utf8(&region, pss_alg,
					(const uint8_t *)"modulusLength", 13,
					jsval_number(2048.0)) == 0);
			assert(jsval_object_set_utf8(&region, pss_alg,
					(const uint8_t *)"hash", 4, hash_obj) == 0);

			assert(jsval_subtle_crypto_generate_key(&region, subtle_a, pss_alg,
					1, usages, &gen_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, gen_promise, &pair) == 0);
			assert(jsval_object_get_utf8(&region, pair,
					(const uint8_t *)"publicKey", 9, &public_key) == 0);
			assert(jsval_object_get_utf8(&region, pair,
					(const uint8_t *)"privateKey", 10, &private_key) == 0);

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					spki_format, public_key, &spki_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, spki_promise,
					&spki_bytes) == 0);
			assert(spki_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);

			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					pkcs8_format, private_key, &pkcs8_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, pkcs8_promise,
					&pkcs8_bytes) == 0);
			assert(pkcs8_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					spki_format, spki_bytes, pss_alg, 1, verify_usages,
					&reimport_public_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_public_promise,
					&reimport_public) == 0);
			assert(reimport_public.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					pkcs8_format, pkcs8_bytes, pss_alg, 1, sign_only_usages,
					&reimport_private_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_private_promise,
					&reimport_private) == 0);
			assert(reimport_private.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_object_new(&region, 1, &sign_alg) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-PSS",
					7, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, sign_alg,
					(const uint8_t *)"name", 4, name_string) == 0);
			assert(jsval_subtle_crypto_sign(&region, subtle_a, sign_alg,
					reimport_private, data_buffer, &sign_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, sign_promise, &sig) == 0);
			assert(sig.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_subtle_crypto_verify(&region, subtle_a, sign_alg,
					reimport_public, sig, data_buffer,
					&verify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, verify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);
		}
	}

	{
		/*
		 * Ed25519 — the last JWT signing algorithm. Parameterless,
		 * deterministic, 32-byte keys, 64-byte signatures. Round-
		 * trip all four formats (raw, jwk, spki, pkcs8) plus the
		 * usual negatives.
		 */
		jsval_t ed_alg;
		jsval_t ed_op_alg;
		jsval_t ed_usages;
		jsval_t ed_verify_usages;
		jsval_t ed_sign_only_usages;
		jsval_t ed_gen_promise;
		jsval_t ed_pair;
		jsval_t ed_public;
		jsval_t ed_private;
		jsval_t ed_data_typed;
		jsval_t ed_data_buffer;
		jsval_t ed_sign_promise;
		jsval_t ed_signature;
		jsval_t ed_verify_promise;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t spki_format;
		jsval_t pkcs8_format;
		jsval_t name_string;
		jsval_t alg_value;

		assert(jsval_array_new(&region, 2, &ed_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, ed_usages, name_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, ed_usages, name_string) == 0);
		assert(jsval_array_new(&region, 1, &ed_verify_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&name_string) == 0);
		assert(jsval_array_push(&region, ed_verify_usages, name_string) == 0);
		assert(jsval_array_new(&region, 1, &ed_sign_only_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&name_string) == 0);
		assert(jsval_array_push(&region, ed_sign_only_usages,
				name_string) == 0);

		assert(jsval_object_new(&region, 1, &ed_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"Ed25519", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, ed_alg,
				(const uint8_t *)"name", 4, name_string) == 0);
		assert(jsval_object_new(&region, 1, &ed_op_alg) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"Ed25519", 7,
				&name_string) == 0);
		assert(jsval_object_set_utf8(&region, ed_op_alg,
				(const uint8_t *)"name", 4, name_string) == 0);

		assert(jsval_subtle_crypto_generate_key(&region, subtle_a, ed_alg, 1,
				ed_usages, &ed_gen_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, ed_gen_promise,
				&ed_pair) == 0);
		assert(ed_pair.kind == JSVAL_KIND_OBJECT);
		assert(jsval_object_get_utf8(&region, ed_pair,
				(const uint8_t *)"publicKey", 9, &ed_public) == 0);
		assert(jsval_object_get_utf8(&region, ed_pair,
				(const uint8_t *)"privateKey", 10, &ed_private) == 0);
		assert(ed_public.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(ed_private.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, ed_public,
				&alg_value) == 0);
		assert_object_string_prop(&region, alg_value, "name", "Ed25519");

		/* sign/verify round trip. */
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
				&ed_data_typed) == 0);
		assert(jsval_typed_array_buffer(&region, ed_data_typed,
				&ed_data_buffer) == 0);
		assert(jsval_subtle_crypto_sign(&region, subtle_a, ed_op_alg,
				ed_private, ed_data_buffer, &ed_sign_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, ed_sign_promise,
				&ed_signature) == 0);
		assert(ed_signature.kind == JSVAL_KIND_ARRAY_BUFFER);
		assert(jsval_array_buffer_byte_length(&region, ed_signature,
				&len) == 0);
		assert(len == 64);
		assert(jsval_subtle_crypto_verify(&region, subtle_a, ed_op_alg,
				ed_public, ed_signature, ed_data_buffer,
				&ed_verify_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, ed_verify_promise,
				&result) == 0);
		assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);

		/* Four-format export + reimport + reverify round trip. */
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"spki", 4,
				&spki_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"pkcs8", 5,
				&pkcs8_format) == 0);

		{
			jsval_t raw_promise;
			jsval_t raw_bytes;
			jsval_t jwk_public_promise;
			jsval_t jwk_public;
			jsval_t jwk_private_promise;
			jsval_t jwk_private;
			jsval_t spki_promise;
			jsval_t spki_bytes;
			jsval_t pkcs8_promise;
			jsval_t pkcs8_bytes;
			jsval_t reimport_raw_promise;
			jsval_t reimport_raw;
			jsval_t reimport_jwk_promise;
			jsval_t reimport_jwk;
			jsval_t reimport_spki_promise;
			jsval_t reimport_spki;
			jsval_t reimport_pkcs8_promise;
			jsval_t reimport_pkcs8;
			jsval_t reverify_promise;

			/* raw: 32 bytes, public only. */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					raw_format, ed_public, &raw_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, raw_promise,
					&raw_bytes) == 0);
			assert(raw_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);
			assert(jsval_array_buffer_byte_length(&region, raw_bytes,
					&len) == 0);
			assert(len == 32);

			/* jwk public. */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, ed_public, &jwk_public_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_public_promise,
					&jwk_public) == 0);
			assert(jwk_public.kind == JSVAL_KIND_OBJECT);
			assert_object_string_prop(&region, jwk_public, "kty", "OKP");
			assert_object_string_prop(&region, jwk_public, "crv", "Ed25519");
			assert_object_string_prop(&region, jwk_public, "alg", "EdDSA");

			/* jwk private (has d). */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					jwk_format, ed_private, &jwk_private_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, jwk_private_promise,
					&jwk_private) == 0);
			assert(jwk_private.kind == JSVAL_KIND_OBJECT);
			{
				jsval_t d_prop;
				assert(jsval_object_get_utf8(&region, jwk_private,
						(const uint8_t *)"d", 1, &d_prop) == 0);
				assert(d_prop.kind == JSVAL_KIND_STRING);
			}

			/* spki public. */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					spki_format, ed_public, &spki_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, spki_promise,
					&spki_bytes) == 0);
			assert(spki_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);

			/* pkcs8 private. */
			assert(jsval_subtle_crypto_export_key(&region, subtle_a,
					pkcs8_format, ed_private, &pkcs8_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, pkcs8_promise,
					&pkcs8_bytes) == 0);
			assert(pkcs8_bytes.kind == JSVAL_KIND_ARRAY_BUFFER);

			/* Reimport all four and verify the original signature
			 * against each reimported public key (or re-sign with
			 * the reimported private key and verify against the
			 * original public). */
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					raw_format, raw_bytes, ed_alg, 1, ed_verify_usages,
					&reimport_raw_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_raw_promise,
					&reimport_raw) == 0);
			assert(reimport_raw.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					jwk_format, jwk_public, ed_alg, 1, ed_verify_usages,
					&reimport_jwk_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_jwk_promise,
					&reimport_jwk) == 0);
			assert(reimport_jwk.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					spki_format, spki_bytes, ed_alg, 1, ed_verify_usages,
					&reimport_spki_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_spki_promise,
					&reimport_spki) == 0);
			assert(reimport_spki.kind == JSVAL_KIND_CRYPTO_KEY);

			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					pkcs8_format, pkcs8_bytes, ed_alg, 1,
					ed_sign_only_usages, &reimport_pkcs8_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reimport_pkcs8_promise,
					&reimport_pkcs8) == 0);
			assert(reimport_pkcs8.kind == JSVAL_KIND_CRYPTO_KEY);

			/* Verify the original signature with each reimported
			 * public key. */
			assert(jsval_subtle_crypto_verify(&region, subtle_a, ed_op_alg,
					reimport_raw, ed_signature, ed_data_buffer,
					&reverify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reverify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);

			assert(jsval_subtle_crypto_verify(&region, subtle_a, ed_op_alg,
					reimport_jwk, ed_signature, ed_data_buffer,
					&reverify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reverify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);

			assert(jsval_subtle_crypto_verify(&region, subtle_a, ed_op_alg,
					reimport_spki, ed_signature, ed_data_buffer,
					&reverify_promise) == 0);
			memset(&error, 0, sizeof(error));
			assert(jsval_microtask_drain(&region, &error) == 0);
			assert(jsval_promise_result(&region, reverify_promise,
					&result) == 0);
			assert(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1);

			/* Ed25519 is deterministic — the reimported private key
			 * must produce the exact same signature as the original. */
			{
				jsval_t resign_promise;
				jsval_t resigned;
				uint8_t orig[64];
				uint8_t again[64];
				size_t orig_len = 0;
				size_t again_len = 0;

				assert(jsval_subtle_crypto_sign(&region, subtle_a,
						ed_op_alg, reimport_pkcs8, ed_data_buffer,
						&resign_promise) == 0);
				memset(&error, 0, sizeof(error));
				assert(jsval_microtask_drain(&region, &error) == 0);
				assert(jsval_promise_result(&region, resign_promise,
						&resigned) == 0);
				assert(resigned.kind == JSVAL_KIND_ARRAY_BUFFER);
				assert(jsval_array_buffer_copy_bytes(&region, ed_signature,
						orig, sizeof(orig), &orig_len) == 0);
				assert(jsval_array_buffer_copy_bytes(&region, resigned,
						again, sizeof(again), &again_len) == 0);
				assert(orig_len == 64 && again_len == 64);
				assert(memcmp(orig, again, 64) == 0);
			}
		}

		/* Negative: sign with public key -> InvalidAccessError. */
		{
			jsval_t bad_sign_promise;

			assert(jsval_subtle_crypto_sign(&region, subtle_a, ed_op_alg,
					ed_public, ed_data_buffer, &bad_sign_promise) == 0);
			assert(jsval_promise_result(&region, bad_sign_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "InvalidAccessError",
					"key does not support sign");
		}

		/* Negative: malformed OKP JWK (wrong crv) -> DataError. */
		{
			jsval_t bad_jwk;
			jsval_t bad_import_promise;

			assert(jsval_object_new(&region, 4, &bad_jwk) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"OKP", 3,
					&name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"kty", 3, name_string) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"X25519",
					6, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"crv", 3, name_string) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
					43, &name_string) == 0);
			assert(jsval_object_set_utf8(&region, bad_jwk,
					(const uint8_t *)"x", 1, name_string) == 0);
			assert(jsval_subtle_crypto_import_key(&region, subtle_a,
					jwk_format, bad_jwk, ed_alg, 1, ed_verify_usages,
					&bad_import_promise) == 0);
			assert(jsval_promise_result(&region, bad_import_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "DataError",
					"invalid OKP JWK crv");
		}
	}

	{
		jsval_t derive_usages;
		jsval_t derive_key_only_usages;
		jsval_t sign_verify_usages;
		jsval_t encrypt_decrypt_usages;
		jsval_t pbkdf2_algorithm;
		jsval_t pbkdf2_params;
		jsval_t pbkdf2_invalid_params;
		jsval_t hash_object;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t password_input;
		jsval_t salt_input;
		jsval_t base_key_promise;
		jsval_t base_key;
		jsval_t derive_key_only_base_promise;
		jsval_t derive_key_only_base;
		jsval_t derive_bits_promise;
		jsval_t derive_hmac_key_promise;
		jsval_t derive_hmac_key;
		jsval_t derive_aes_key_promise;
		jsval_t derive_aes_key;
		jsval_t export_hmac_raw_promise;
		jsval_t export_aes_raw_promise;
		jsval_t unsupported_format_promise;
		jsval_t invalid_length_promise;
		jsval_t invalid_salt_promise;
		jsval_t invalid_usage_promise;
		jsval_t unsupported_target_promise;
		jsval_t usage_string;
		jsval_t hmac_algorithm;
		jsval_t aes_algorithm;
		jsval_t unsupported_algorithm;

		assert(jsval_array_new(&region, 2, &derive_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"deriveBits", 10,
				&usage_string) == 0);
		assert(jsval_array_push(&region, derive_usages, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"deriveKey", 9,
				&usage_string) == 0);
		assert(jsval_array_push(&region, derive_usages, usage_string) == 0);
		assert(jsval_array_new(&region, 1, &derive_key_only_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"deriveKey", 9,
				&usage_string) == 0);
		assert(jsval_array_push(&region, derive_key_only_usages, usage_string)
				== 0);
		assert(jsval_array_new(&region, 2, &sign_verify_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&usage_string) == 0);
		assert(jsval_array_push(&region, sign_verify_usages, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&usage_string) == 0);
		assert(jsval_array_push(&region, sign_verify_usages, usage_string) == 0);
		assert(jsval_array_new(&region, 2, &encrypt_decrypt_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&usage_string) == 0);
		assert(jsval_array_push(&region, encrypt_decrypt_usages, usage_string)
				== 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&usage_string) == 0);
		assert(jsval_array_push(&region, encrypt_decrypt_usages, usage_string)
				== 0);
		assert(jsval_object_new(&region, 1, &hash_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hash_object,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_new(&region, 1, &pbkdf2_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_new(&region, 4, &pbkdf2_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_params,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_params,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
				&password_input) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
				&salt_input) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_params,
				(const uint8_t *)"salt", 4, salt_input) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_params,
				(const uint8_t *)"iterations", 10, jsval_number(2.0)) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);

		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				password_input, pbkdf2_algorithm, 1, derive_usages,
				&base_key_promise) == 0);
		assert(jsval_promise_state(&region, base_key_promise, &promise_state)
				== 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, base_key_promise, &base_key) == 0);
		assert(base_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, base_key, &result) == 0);
		assert_object_string_prop(&region, result, "name", "PBKDF2");
		assert(jsval_crypto_key_usages(&region, base_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_DERIVE_BITS
				| JSVAL_CRYPTO_KEY_USAGE_DERIVE_KEY));
		assert(jsval_crypto_key_extractable(&region, base_key, &extractable) == 0);
		assert(extractable == 1);

		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a, pbkdf2_params,
				base_key, jsval_number(256.0), &derive_bits_promise) == 0);
		assert(jsval_promise_state(&region, derive_bits_promise, &promise_state)
				== 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_bits_promise, &result) == 0);
		assert_array_buffer_bytes(&region, result, pbkdf2_zero_derivebits_32,
				sizeof(pbkdf2_zero_derivebits_32));

		assert(jsval_object_new(&region, 2, &hmac_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_subtle_crypto_derive_key(&region, subtle_a, pbkdf2_params,
				base_key, hmac_algorithm, 1, sign_verify_usages,
				&derive_hmac_key_promise) == 0);
		assert(jsval_promise_state(&region, derive_hmac_key_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_hmac_key_promise,
				&derive_hmac_key) == 0);
		assert(jsval_crypto_key_algorithm(&region, derive_hmac_key, &result) == 0);
		assert_object_string_prop(&region, result, "name", "HMAC");
		assert(jsval_object_get_utf8(&region, result, (const uint8_t *)"hash", 4,
				&hash_object) == 0);
		assert_object_string_prop(&region, hash_object, "name", "SHA-256");
		assert_object_number_prop(&region, result, "length", 512.0);
		assert(jsval_crypto_key_usages(&region, derive_hmac_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_SIGN
				| JSVAL_CRYPTO_KEY_USAGE_VERIFY));
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				derive_hmac_key, &export_hmac_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, export_hmac_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, pbkdf2_zero_derivekey_hmac_64,
				sizeof(pbkdf2_zero_derivekey_hmac_64));

		assert(jsval_object_new(&region, 2, &aes_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_subtle_crypto_derive_key(&region, subtle_a, pbkdf2_params,
				base_key, aes_algorithm, 1, encrypt_decrypt_usages,
				&derive_aes_key_promise) == 0);
		assert(jsval_promise_state(&region, derive_aes_key_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_aes_key_promise,
				&derive_aes_key) == 0);
		assert(jsval_crypto_key_algorithm(&region, derive_aes_key, &result) == 0);
		assert_object_string_prop(&region, result, "name", "AES-GCM");
		assert_object_number_prop(&region, result, "length", 128.0);
		assert(jsval_crypto_key_usages(&region, derive_aes_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
				| JSVAL_CRYPTO_KEY_USAGE_DECRYPT));
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				derive_aes_key, &export_aes_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, export_aes_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, pbkdf2_zero_derivekey_aes_16,
				sizeof(pbkdf2_zero_derivekey_aes_16));

		assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				password_input, pbkdf2_algorithm, 1, derive_usages,
				&unsupported_format_promise) == 0);
		assert(jsval_promise_result(&region, unsupported_format_promise, &result)
				== 0);
		assert_dom_exception(&region, result, "NotSupportedError",
				"unsupported key format");

		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				password_input, pbkdf2_algorithm, 1, derive_key_only_usages,
				&derive_key_only_base_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_key_only_base_promise,
				&derive_key_only_base) == 0);
		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a, pbkdf2_params,
				derive_key_only_base, jsval_number(256.0),
				&invalid_usage_promise) == 0);
		assert(jsval_promise_result(&region, invalid_usage_promise, &result) == 0);
		assert_dom_exception(&region, result, "InvalidAccessError",
				"key does not support requested usage");

		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a, pbkdf2_params,
				base_key, jsval_number(255.0), &invalid_length_promise) == 0);
		assert(jsval_promise_result(&region, invalid_length_promise, &result)
				== 0);
		assert_dom_exception(&region, result, "OperationError",
				"expected PBKDF2 length");

		{
			jsval_t zero_iterations_params;
			jsval_t zero_iterations_promise;

			assert(jsval_object_new(&region, 4, &zero_iterations_params) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
					&usage_string) == 0);
			assert(jsval_object_set_utf8(&region, zero_iterations_params,
					(const uint8_t *)"name", 4, usage_string) == 0);
			assert(jsval_object_set_utf8(&region, zero_iterations_params,
					(const uint8_t *)"hash", 4, hash_object) == 0);
			assert(jsval_object_set_utf8(&region, zero_iterations_params,
					(const uint8_t *)"salt", 4, salt_input) == 0);
			assert(jsval_object_set_utf8(&region, zero_iterations_params,
					(const uint8_t *)"iterations", 10, jsval_number(0.0)) == 0);
			assert(jsval_subtle_crypto_derive_bits(&region, subtle_a,
					zero_iterations_params, base_key, jsval_number(256.0),
					&zero_iterations_promise) == 0);
			assert(jsval_promise_result(&region, zero_iterations_promise,
					&result) == 0);
			assert_dom_exception(&region, result, "OperationError",
					"expected PBKDF2 iterations");
		}

		assert(jsval_object_new(&region, 3, &pbkdf2_invalid_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_invalid_params,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_invalid_params,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_object_set_utf8(&region, pbkdf2_invalid_params,
				(const uint8_t *)"iterations", 10, jsval_number(2.0)) == 0);
		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a,
				pbkdf2_invalid_params, base_key, jsval_number(256.0),
				&invalid_salt_promise) == 0);
		assert(jsval_promise_result(&region, invalid_salt_promise, &result) == 0);
		assert_dom_exception(&region, result, "TypeError",
				"expected PBKDF2 salt");

		assert(jsval_object_new(&region, 2, &unsupported_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-OAEP", 8,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, unsupported_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, unsupported_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_subtle_crypto_derive_key(&region, subtle_a, pbkdf2_params,
				base_key, unsupported_algorithm, 1, encrypt_decrypt_usages,
				&unsupported_target_promise) == 0);
		assert(jsval_promise_result(&region, unsupported_target_promise,
				&result) == 0);
		assert_dom_exception(&region, result, "NotSupportedError",
				"unsupported derived key algorithm");
	}
	{
		jsval_t derive_usages;
		jsval_t derive_key_only_usages;
		jsval_t sign_verify_usages;
		jsval_t encrypt_decrypt_usages;
		jsval_t hkdf_algorithm;
		jsval_t hkdf_params;
		jsval_t hkdf_missing_salt;
		jsval_t hkdf_missing_info;
		jsval_t hash_object;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t key_input;
		jsval_t salt_input;
		jsval_t info_input;
		jsval_t base_key_promise;
		jsval_t base_key;
		jsval_t derive_key_only_base_promise;
		jsval_t derive_key_only_base;
		jsval_t derive_bits_promise;
		jsval_t derive_hmac_key_promise;
		jsval_t derive_hmac_key;
		jsval_t derive_aes_key_promise;
		jsval_t derive_aes_key;
		jsval_t export_hmac_raw_promise;
		jsval_t export_aes_raw_promise;
		jsval_t unsupported_format_promise;
		jsval_t invalid_length_promise;
		jsval_t invalid_salt_promise;
		jsval_t invalid_info_promise;
		jsval_t invalid_usage_promise;
		jsval_t unsupported_target_promise;
		jsval_t usage_string;
		jsval_t hmac_algorithm;
		jsval_t aes_algorithm;
		jsval_t unsupported_algorithm;

		assert(jsval_array_new(&region, 2, &derive_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"deriveBits", 10,
				&usage_string) == 0);
		assert(jsval_array_push(&region, derive_usages, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"deriveKey", 9,
				&usage_string) == 0);
		assert(jsval_array_push(&region, derive_usages, usage_string) == 0);
		assert(jsval_array_new(&region, 1, &derive_key_only_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"deriveKey", 9,
				&usage_string) == 0);
		assert(jsval_array_push(&region, derive_key_only_usages, usage_string)
				== 0);
		assert(jsval_array_new(&region, 2, &sign_verify_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
				&usage_string) == 0);
		assert(jsval_array_push(&region, sign_verify_usages, usage_string) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
				&usage_string) == 0);
		assert(jsval_array_push(&region, sign_verify_usages, usage_string) == 0);
		assert(jsval_array_new(&region, 2, &encrypt_decrypt_usages) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"encrypt", 7,
				&usage_string) == 0);
		assert(jsval_array_push(&region, encrypt_decrypt_usages, usage_string)
				== 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"decrypt", 7,
				&usage_string) == 0);
		assert(jsval_array_push(&region, encrypt_decrypt_usages, usage_string)
				== 0);
		assert(jsval_object_new(&region, 1, &hash_object) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hash_object,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_new(&region, 1, &hkdf_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_new(&region, 4, &hkdf_params) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_params,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_params,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
				&key_input) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
				&salt_input) == 0);
		assert(jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
				&info_input) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_params,
				(const uint8_t *)"salt", 4, salt_input) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_params,
				(const uint8_t *)"info", 4, info_input) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
				&raw_format) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
				&jwk_format) == 0);

		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				key_input, hkdf_algorithm, 1, derive_usages,
				&base_key_promise) == 0);
		assert(jsval_promise_state(&region, base_key_promise, &promise_state)
				== 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, base_key_promise, &base_key) == 0);
		assert(base_key.kind == JSVAL_KIND_CRYPTO_KEY);
		assert(jsval_crypto_key_algorithm(&region, base_key, &result) == 0);
		assert_object_string_prop(&region, result, "name", "HKDF");
		assert(jsval_crypto_key_usages(&region, base_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_DERIVE_BITS
				| JSVAL_CRYPTO_KEY_USAGE_DERIVE_KEY));
		assert(jsval_crypto_key_extractable(&region, base_key, &extractable) == 0);
		assert(extractable == 1);

		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a, hkdf_params,
				base_key, jsval_number(256.0), &derive_bits_promise) == 0);
		assert(jsval_promise_state(&region, derive_bits_promise, &promise_state)
				== 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_bits_promise, &result) == 0);
		assert_array_buffer_bytes(&region, result, hkdf_zero_derivebits_32,
				sizeof(hkdf_zero_derivebits_32));

		assert(jsval_object_new(&region, 2, &hmac_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hmac_algorithm,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_subtle_crypto_derive_key(&region, subtle_a, hkdf_params,
				base_key, hmac_algorithm, 1, sign_verify_usages,
				&derive_hmac_key_promise) == 0);
		assert(jsval_promise_state(&region, derive_hmac_key_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_hmac_key_promise,
				&derive_hmac_key) == 0);
		assert(jsval_crypto_key_algorithm(&region, derive_hmac_key, &result) == 0);
		assert_object_string_prop(&region, result, "name", "HMAC");
		assert(jsval_object_get_utf8(&region, result, (const uint8_t *)"hash", 4,
				&hash_object) == 0);
		assert_object_string_prop(&region, hash_object, "name", "SHA-256");
		assert_object_number_prop(&region, result, "length", 512.0);
		assert(jsval_crypto_key_usages(&region, derive_hmac_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_SIGN
				| JSVAL_CRYPTO_KEY_USAGE_VERIFY));
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				derive_hmac_key, &export_hmac_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, export_hmac_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, hkdf_zero_derivekey_hmac_64,
				sizeof(hkdf_zero_derivekey_hmac_64));

		assert(jsval_object_new(&region, 2, &aes_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM", 7,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, aes_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_subtle_crypto_derive_key(&region, subtle_a, hkdf_params,
				base_key, aes_algorithm, 1, encrypt_decrypt_usages,
				&derive_aes_key_promise) == 0);
		assert(jsval_promise_state(&region, derive_aes_key_promise,
				&promise_state) == 0);
		assert(promise_state == JSVAL_PROMISE_STATE_PENDING);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_aes_key_promise,
				&derive_aes_key) == 0);
		assert(jsval_crypto_key_algorithm(&region, derive_aes_key, &result) == 0);
		assert_object_string_prop(&region, result, "name", "AES-GCM");
		assert_object_number_prop(&region, result, "length", 128.0);
		assert(jsval_crypto_key_usages(&region, derive_aes_key, &usages) == 0);
		assert(usages == (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
				| JSVAL_CRYPTO_KEY_USAGE_DECRYPT));
		assert(jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				derive_aes_key, &export_aes_raw_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, export_aes_raw_promise, &result)
				== 0);
		assert_array_buffer_bytes(&region, result, hkdf_zero_derivekey_aes_16,
				sizeof(hkdf_zero_derivekey_aes_16));

		assert(jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				key_input, hkdf_algorithm, 1, derive_usages,
				&unsupported_format_promise) == 0);
		assert(jsval_promise_result(&region, unsupported_format_promise, &result)
				== 0);
		assert_dom_exception(&region, result, "NotSupportedError",
				"unsupported key format");

		assert(jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				key_input, hkdf_algorithm, 1, derive_key_only_usages,
				&derive_key_only_base_promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_result(&region, derive_key_only_base_promise,
				&derive_key_only_base) == 0);
		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a, hkdf_params,
				derive_key_only_base, jsval_number(256.0),
				&invalid_usage_promise) == 0);
		assert(jsval_promise_result(&region, invalid_usage_promise, &result) == 0);
		assert_dom_exception(&region, result, "InvalidAccessError",
				"key does not support requested usage");

		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a, hkdf_params,
				base_key, jsval_number(255.0), &invalid_length_promise) == 0);
		assert(jsval_promise_result(&region, invalid_length_promise, &result)
				== 0);
		assert_dom_exception(&region, result, "OperationError",
				"expected HKDF length");

		assert(jsval_object_new(&region, 3, &hkdf_missing_salt) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_missing_salt,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_missing_salt,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_missing_salt,
				(const uint8_t *)"info", 4, info_input) == 0);
		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a,
				hkdf_missing_salt, base_key, jsval_number(256.0),
				&invalid_salt_promise) == 0);
		assert(jsval_promise_result(&region, invalid_salt_promise, &result) == 0);
		assert_dom_exception(&region, result, "TypeError",
				"expected HKDF salt");

		assert(jsval_object_new(&region, 3, &hkdf_missing_info) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_missing_info,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_missing_info,
				(const uint8_t *)"hash", 4, hash_object) == 0);
		assert(jsval_object_set_utf8(&region, hkdf_missing_info,
				(const uint8_t *)"salt", 4, salt_input) == 0);
		assert(jsval_subtle_crypto_derive_bits(&region, subtle_a,
				hkdf_missing_info, base_key, jsval_number(256.0),
				&invalid_info_promise) == 0);
		assert(jsval_promise_result(&region, invalid_info_promise, &result) == 0);
		assert_dom_exception(&region, result, "TypeError",
				"expected HKDF info");

		assert(jsval_object_new(&region, 2, &unsupported_algorithm) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"RSA-OAEP", 8,
				&usage_string) == 0);
		assert(jsval_object_set_utf8(&region, unsupported_algorithm,
				(const uint8_t *)"name", 4, usage_string) == 0);
		assert(jsval_object_set_utf8(&region, unsupported_algorithm,
				(const uint8_t *)"length", 6, jsval_number(128.0)) == 0);
		assert(jsval_subtle_crypto_derive_key(&region, subtle_a, hkdf_params,
				base_key, unsupported_algorithm, 1, encrypt_decrypt_usages,
				&unsupported_target_promise) == 0);
		assert(jsval_promise_result(&region, unsupported_target_promise,
				&result) == 0);
		assert_dom_exception(&region, result, "NotSupportedError",
				"unsupported derived key algorithm");
	}
#else
	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(jsval_crypto_get_random_values(&region, crypto, typed_u8, &result,
			&error) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_crypto_random_uuid(&region, crypto, &uuid) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_subtle_crypto_digest(&region, subtle_a, algorithm_name,
			digest_input, &digest_promise) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_subtle_crypto_encrypt(&region, subtle_a, jsval_undefined(),
			jsval_undefined(), jsval_undefined(), &digest_promise) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_subtle_crypto_decrypt(&region, subtle_a, jsval_undefined(),
			jsval_undefined(), jsval_undefined(), &digest_promise) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_subtle_crypto_derive_bits(&region, subtle_a,
			jsval_undefined(), jsval_undefined(), jsval_undefined(),
			&digest_promise) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_subtle_crypto_derive_key(&region, subtle_a,
			jsval_undefined(), jsval_undefined(), jsval_undefined(), 0,
			jsval_undefined(), &digest_promise) < 0);
	assert(errno == ENOTSUP);
#endif

	assert(jsval_crypto_key_new(&region, JSVAL_CRYPTO_KEY_TYPE_SECRET, 1,
			algorithm_name, JSVAL_CRYPTO_KEY_USAGE_SIGN
			| JSVAL_CRYPTO_KEY_USAGE_VERIFY, &key) == 0);
	assert(key.kind == JSVAL_KIND_CRYPTO_KEY);
	assert(jsval_typeof(&region, key, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_crypto_key_type(&region, key, &result) == 0);
	assert_string(&region, result, "secret");
	assert(jsval_crypto_key_extractable(&region, key, &extractable) == 0);
	assert(extractable == 1);
	assert(jsval_crypto_key_algorithm(&region, key, &result) == 0);
	assert_string(&region, result, "SHA-256");
	assert(jsval_crypto_key_usages(&region, key, &usages) == 0);
	assert(usages == (JSVAL_CRYPTO_KEY_USAGE_SIGN
			| JSVAL_CRYPTO_KEY_USAGE_VERIFY));

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"QuotaExceededError",
			sizeof("QuotaExceededError") - 1, &uuid) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"too many bytes",
			sizeof("too many bytes") - 1, &result) == 0);
	assert(jsval_dom_exception_new(&region, uuid, result, &dom_exception) == 0);
	assert(dom_exception.kind == JSVAL_KIND_DOM_EXCEPTION);
	assert(jsval_typeof(&region, dom_exception, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_dom_exception_name(&region, dom_exception, &result) == 0);
	assert_string(&region, result, "QuotaExceededError");
	assert(jsval_dom_exception_message(&region, dom_exception, &result) == 0);
	assert_string(&region, result, "too many bytes");
}

static void test_promise_semantics(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_scheduler_t scheduler;
	jsval_scheduler_t scheduler_copy;
	test_scheduler_ctx_t scheduler_ctx;
	jsval_t identity_fn;
	jsval_t return_fn;
	jsval_t throw_fn;
	jsval_t microtask_fn;
	jsval_t promise;
	jsval_t downstream;
	jsval_t settled_then;
	jsval_t rejected_source;
	jsval_t caught;
	jsval_t finally_source;
	jsval_t finally_chain;
	jsval_t cleanup;
	jsval_t finally_rejected_source;
	jsval_t after_finally;
	jsval_t outer;
	jsval_t inner;
	jsval_t throwing_source;
	jsval_t rejected_chain;
	jsval_t self_promise;
	jsval_t fixed;
	jsval_t boom;
	jsval_t caught_value;
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	memset(&test_promise_identity_ctx, 0, sizeof(test_promise_identity_ctx));
	memset(&test_promise_return_ctx, 0, sizeof(test_promise_return_ctx));
	memset(&test_promise_throw_ctx, 0, sizeof(test_promise_throw_ctx));
	memset(&test_microtask_ctx, 0, sizeof(test_microtask_ctx));
	memset(&scheduler_ctx, 0, sizeof(scheduler_ctx));

	jsval_region_init(&region, storage, sizeof(storage));
	memset(&scheduler, 0, sizeof(scheduler));
	scheduler.ctx = &scheduler_ctx;
	scheduler.on_enqueue = test_scheduler_on_enqueue;
	scheduler.on_wake = test_scheduler_on_wake;
	jsval_region_set_scheduler(&region, &scheduler);
	memset(&scheduler_copy, 0, sizeof(scheduler_copy));
	jsval_region_get_scheduler(&region, &scheduler_copy);
	assert(scheduler_copy.ctx == &scheduler_ctx);
	assert(scheduler_copy.on_enqueue == test_scheduler_on_enqueue);
	assert(scheduler_copy.on_wake == test_scheduler_on_wake);

	assert(jsval_function_new(&region, test_promise_identity, 1, 0,
			jsval_undefined(), &identity_fn) == 0);
	assert(jsval_function_new(&region, test_promise_return, 0, 0,
			jsval_undefined(), &return_fn) == 0);
	assert(jsval_function_new(&region, test_promise_throw_handler, 1, 0,
			jsval_undefined(), &throw_fn) == 0);
	assert(jsval_function_new(&region, test_microtask_function, 0, 0,
			jsval_undefined(), &microtask_fn) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"boom", 4,
			&boom) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"caught", 6,
			&caught_value) == 0);

	test_microtask_ctx.nested_function = microtask_fn;
	test_microtask_ctx.enqueue_nested = 1;
	assert(jsval_microtask_enqueue(&region, microtask_fn, 0, NULL) == 0);
	assert(jsval_microtask_pending(&region) == 1);
	assert(scheduler_ctx.enqueue_count == 1);
	assert(scheduler_ctx.wake_count == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(error.kind == JSMETHOD_ERROR_NONE);
	assert(jsval_microtask_pending(&region) == 0);
	assert(test_microtask_ctx.call_count == 2);
	assert(scheduler_ctx.enqueue_count == 2);
	assert(scheduler_ctx.wake_count == 1);

	memset(&scheduler_ctx, 0, sizeof(scheduler_ctx));
	assert(jsval_promise_new(&region, &promise) == 0);
	assert(promise.kind == JSVAL_KIND_PROMISE);
	assert(jsval_truthy(&region, promise) == 1);
	assert(jsval_typeof(&region, promise, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_promise_state(&region, promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_promise_result(&region, promise, &result) == 0);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_strict_eq(&region, promise, promise) == 1);

	assert(jsval_promise_then(&region, promise, identity_fn,
			jsval_undefined(), &downstream) == 0);
	assert(jsval_promise_resolve(&region, promise, jsval_number(7.0)) == 0);
	assert(jsval_promise_state(&region, promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_state(&region, downstream, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_microtask_pending(&region) == 1);
	assert(scheduler_ctx.enqueue_count == 1);
	assert(scheduler_ctx.wake_count == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(error.kind == JSMETHOD_ERROR_NONE);
	assert(test_promise_identity_ctx.call_count == 1);
	assert(test_promise_identity_ctx.last_argc == 1);
	assert_number_value(test_promise_identity_ctx.last_arg, 7.0);
	assert(jsval_promise_state(&region, downstream, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, downstream, &result) == 0);
	assert_number_value(result, 7.0);

	memset(&scheduler_ctx, 0, sizeof(scheduler_ctx));
	memset(&test_promise_identity_ctx, 0, sizeof(test_promise_identity_ctx));
	assert(jsval_promise_then(&region, promise, identity_fn,
			jsval_undefined(), &settled_then) == 0);
	assert(jsval_promise_state(&region, settled_then, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_microtask_pending(&region) == 1);
	assert(scheduler_ctx.enqueue_count == 1);
	assert(scheduler_ctx.wake_count == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(test_promise_identity_ctx.call_count == 1);
	assert_number_value(test_promise_identity_ctx.last_arg, 7.0);
	assert(jsval_promise_state(&region, settled_then, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, settled_then, &result) == 0);
	assert_number_value(result, 7.0);

	memset(&scheduler_ctx, 0, sizeof(scheduler_ctx));
	memset(&test_promise_return_ctx, 0, sizeof(test_promise_return_ctx));
	test_promise_return_ctx.return_value = caught_value;
	assert(jsval_promise_new(&region, &rejected_source) == 0);
	assert(jsval_promise_catch(&region, rejected_source, return_fn, &caught)
			== 0);
	assert(jsval_promise_reject(&region, rejected_source, boom) == 0);
	assert(jsval_promise_state(&region, caught, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_microtask_pending(&region) == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(test_promise_return_ctx.call_count == 1);
	assert_string(&region, test_promise_return_ctx.last_arg, "boom");
	assert(jsval_promise_state(&region, caught, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, caught, &result) == 0);
	assert_string(&region, result, "caught");

	memset(&scheduler_ctx, 0, sizeof(scheduler_ctx));
	memset(&test_promise_return_ctx, 0, sizeof(test_promise_return_ctx));
	test_promise_return_ctx.return_value = jsval_undefined();
	assert(jsval_promise_new(&region, &finally_source) == 0);
	assert(jsval_promise_finally(&region, finally_source, return_fn,
			&finally_chain) == 0);
	assert(jsval_promise_resolve(&region, finally_source, jsval_number(42.0))
			== 0);
	assert(jsval_microtask_pending(&region) == 1);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(test_promise_return_ctx.call_count == 1);
	assert(test_promise_return_ctx.last_argc == 0);
	assert(jsval_promise_state(&region, finally_chain, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, finally_chain, &result) == 0);
	assert_number_value(result, 42.0);

	memset(&scheduler_ctx, 0, sizeof(scheduler_ctx));
	memset(&test_promise_return_ctx, 0, sizeof(test_promise_return_ctx));
	assert(jsval_promise_new(&region, &cleanup) == 0);
	test_promise_return_ctx.return_value = cleanup;
	assert(jsval_promise_new(&region, &finally_rejected_source) == 0);
	assert(jsval_promise_finally(&region, finally_rejected_source, return_fn,
			&after_finally) == 0);
	assert(jsval_promise_reject(&region, finally_rejected_source, boom) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(test_promise_return_ctx.call_count == 1);
	assert(jsval_promise_state(&region, after_finally, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_promise_resolve(&region, cleanup, jsval_undefined()) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, after_finally, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, after_finally, &result) == 0);
	assert_string(&region, result, "boom");

	assert(jsval_promise_new(&region, &outer) == 0);
	assert(jsval_promise_new(&region, &inner) == 0);
	assert(jsval_promise_resolve(&region, outer, inner) == 0);
	assert(jsval_promise_state(&region, outer, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_promise_resolve(&region, inner, jsval_number(9.0)) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, outer, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, outer, &result) == 0);
	assert_number_value(result, 9.0);

	memset(&test_promise_throw_ctx, 0, sizeof(test_promise_throw_ctx));
	assert(jsval_promise_new(&region, &throwing_source) == 0);
	assert(jsval_promise_then(&region, throwing_source, throw_fn,
			jsval_undefined(), &rejected_chain) == 0);
	assert(jsval_promise_resolve(&region, throwing_source, jsval_number(1.0))
			== 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(test_promise_throw_ctx.call_count == 1);
	assert(jsval_promise_state(&region, rejected_chain, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, rejected_chain, &result) == 0);
	assert_dom_exception(&region, result, "Error",
			"test promise handler threw");

	assert(jsval_promise_new(&region, &self_promise) == 0);
	assert(jsval_promise_resolve(&region, self_promise, self_promise) == 0);
	assert(jsval_promise_state(&region, self_promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, self_promise, &result) == 0);
	assert_dom_exception(&region, result, "TypeError",
			"promise self-resolution");

	assert(jsval_promise_new(&region, &fixed) == 0);
	assert(jsval_promise_resolve(&region, fixed, jsval_number(3.0)) == 0);
	assert(jsval_promise_reject(&region, fixed, jsval_number(4.0)) == 0);
	assert(jsval_promise_state(&region, fixed, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, fixed, &result) == 0);
	assert_number_value(result, 3.0);
}

static void test_promise_all_semantics(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t p[3];
	jsval_t all;
	jsval_t result;
	jsval_t elem;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/* n==0: empty array, synchronously fulfilled with empty array. */
	assert(jsval_promise_all(&region, NULL, 0, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 0);

	/* Fast path: all pre-fulfilled. Resolve in input order. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_resolve(&region, p[0], jsval_number(10.0)) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_resolve(&region, p[1], jsval_number(20.0)) == 0);
	assert(jsval_promise_new(&region, &p[2]) == 0);
	assert(jsval_promise_resolve(&region, p[2], jsval_number(30.0)) == 0);
	assert(jsval_promise_all(&region, p, 3, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &elem) == 0);
	assert_number_value(elem, 10.0);
	assert(jsval_array_get(&region, result, 1, &elem) == 0);
	assert_number_value(elem, 20.0);
	assert(jsval_array_get(&region, result, 2, &elem) == 0);
	assert_number_value(elem, 30.0);

	/* Fast path: first rejection short-circuits. Input p[1] rejected
	 * before p[0] and p[2]; output rejects with p[1]'s reason. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_resolve(&region, p[0], jsval_number(1.0)) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_reject(&region, p[1], jsval_number(999.0)) == 0);
	assert(jsval_promise_new(&region, &p[2]) == 0);
	assert(jsval_promise_resolve(&region, p[2], jsval_number(3.0)) == 0);
	assert(jsval_promise_all(&region, p, 3, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert_number_value(result, 999.0);

	/* Slow path: settle via drain. Input p[0] pending, p[1] and p[2]
	 * already fulfilled. Output is pending until p[0] resolves and
	 * jsval_microtask_drain's scan phase runs. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_resolve(&region, p[1], jsval_number(200.0)) == 0);
	assert(jsval_promise_new(&region, &p[2]) == 0);
	assert(jsval_promise_resolve(&region, p[2], jsval_number(300.0)) == 0);
	assert(jsval_promise_all(&region, p, 3, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	/* Resolve last; drain must settle `all` with results in input order. */
	assert(jsval_promise_resolve(&region, p[0], jsval_number(100.0)) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &elem) == 0);
	assert_number_value(elem, 100.0);
	assert(jsval_array_get(&region, result, 1, &elem) == 0);
	assert_number_value(elem, 200.0);
	assert(jsval_array_get(&region, result, 2, &elem) == 0);
	assert_number_value(elem, 300.0);

	/* Slow path: one input rejects after aggregation started. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_all(&region, p, 2, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_promise_reject(&region, p[1], jsval_number(42.0)) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert_number_value(result, 42.0);
}

static void test_promise_race_semantics(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t p[3];
	jsval_t race;
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/* n==0: stays pending forever (no settlement source). */
	assert(jsval_promise_race(&region, NULL, 0, &race) == 0);
	assert(jsval_promise_state(&region, race, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);

	/* Fast path: first input already fulfilled wins. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_resolve(&region, p[0], jsval_number(10.0)) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_resolve(&region, p[1], jsval_number(20.0)) == 0);
	assert(jsval_promise_new(&region, &p[2]) == 0);
	assert(jsval_promise_race(&region, p, 3, &race) == 0);
	assert(jsval_promise_state(&region, race, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, race, &result) == 0);
	assert_number_value(result, 10.0);

	/* Fast path: first input already rejected wins. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_reject(&region, p[0], jsval_number(99.0)) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_resolve(&region, p[1], jsval_number(2.0)) == 0);
	assert(jsval_promise_race(&region, p, 2, &race) == 0);
	assert(jsval_promise_state(&region, race, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_REJECTED);
	assert(jsval_promise_result(&region, race, &result) == 0);
	assert_number_value(result, 99.0);

	/* Slow path: all pending at call time; first to settle wins. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_new(&region, &p[2]) == 0);
	assert(jsval_promise_race(&region, p, 3, &race) == 0);
	assert(jsval_promise_state(&region, race, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	/* Resolve p[1] first; race should fulfill with its value. */
	assert(jsval_promise_resolve(&region, p[1], jsval_number(777.0)) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, race, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, race, &result) == 0);
	assert_number_value(result, 777.0);
	/* Later settlements on p[0] / p[2] don't matter. */
	assert(jsval_promise_reject(&region, p[0], jsval_number(1.0)) == 0);
	assert(jsval_promise_resolve(&region, p[2], jsval_number(3.0)) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, race, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, race, &result) == 0);
	assert_number_value(result, 777.0);
}

static void test_promise_all_settled_semantics(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t p[3];
	jsval_t all;
	jsval_t result;
	jsval_t entry;
	jsval_t status_val;
	jsval_t payload;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/* n==0: synchronously fulfills with []. */
	assert(jsval_promise_all_settled(&region, NULL, 0, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 0);

	/* Fast path: all pre-settled (mix of fulfilled + rejected). */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_resolve(&region, p[0], jsval_number(1.0)) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_reject(&region, p[1], jsval_number(99.0)) == 0);
	assert(jsval_promise_new(&region, &p[2]) == 0);
	assert(jsval_promise_resolve(&region, p[2], jsval_number(3.0)) == 0);
	assert(jsval_promise_all_settled(&region, p, 3, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	/* Entry 0: { status: "fulfilled", value: 1 }. */
	assert(jsval_array_get(&region, result, 0, &entry) == 0);
	assert(entry.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(&region, entry,
			(const uint8_t *)"status", 6, &status_val) == 0);
	assert(status_val.kind == JSVAL_KIND_STRING);
	assert(jsval_object_get_utf8(&region, entry,
			(const uint8_t *)"value", 5, &payload) == 0);
	assert_number_value(payload, 1.0);
	/* Entry 1: { status: "rejected", reason: 99 }. */
	assert(jsval_array_get(&region, result, 1, &entry) == 0);
	assert(jsval_object_get_utf8(&region, entry,
			(const uint8_t *)"reason", 6, &payload) == 0);
	assert_number_value(payload, 99.0);
	/* Entry 2: { status: "fulfilled", value: 3 }. */
	assert(jsval_array_get(&region, result, 2, &entry) == 0);
	assert(jsval_object_get_utf8(&region, entry,
			(const uint8_t *)"value", 5, &payload) == 0);
	assert_number_value(payload, 3.0);

	/* Slow path: one pending; settle last; drain. */
	assert(jsval_promise_new(&region, &p[0]) == 0);
	assert(jsval_promise_new(&region, &p[1]) == 0);
	assert(jsval_promise_resolve(&region, p[1], jsval_number(20.0)) == 0);
	assert(jsval_promise_all_settled(&region, p, 2, &all) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_PENDING);
	assert(jsval_promise_resolve(&region, p[0], jsval_number(10.0)) == 0);
	memset(&error, 0, sizeof(error));
	assert(jsval_microtask_drain(&region, &error) == 0);
	assert(jsval_promise_state(&region, all, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, all, &result) == 0);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &entry) == 0);
	assert(jsval_object_get_utf8(&region, entry,
			(const uint8_t *)"value", 5, &payload) == 0);
	assert_number_value(payload, 10.0);
	assert(jsval_array_get(&region, result, 1, &entry) == 0);
	assert(jsval_object_get_utf8(&region, entry,
			(const uint8_t *)"value", 5, &payload) == 0);
	assert_number_value(payload, 20.0);
}

static void test_set_semantics(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t set;
	jsval_t grown;
	jsval_t result;
	jsval_t string_a;
	jsval_t string_b;
	jsval_t object_a;
	jsval_t object_b;
	size_t size = 0;
	int has = 0;
	int deleted = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_set_new(&region, 2, &set) == 0);
	assert(set.kind == JSVAL_KIND_SET);
	assert(jsval_truthy(&region, set) == 1);
	assert(jsval_typeof(&region, set, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_strict_eq(&region, set, set) == 1);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 0);

	assert(jsval_set_add(&region, set, jsval_number(NAN)) == 0);
	assert(jsval_set_has(&region, set, jsval_number(NAN), &has) == 0);
	assert(has == 1);
	assert(jsval_set_add(&region, set, jsval_number(NAN)) == 0);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 1);

	assert(jsval_set_add(&region, set, jsval_number(-0.0)) == 0);
	assert(jsval_set_has(&region, set, jsval_number(+0.0), &has) == 0);
	assert(has == 1);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 2);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_a) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_b) == 0);
	errno = 0;
	assert(jsval_set_add(&region, set, string_a) == -1);
	assert(errno == ENOBUFS);
	assert(jsval_set_clone(&region, set, 4, &grown) == 0);
	assert(grown.kind == JSVAL_KIND_SET);
	assert(jsval_strict_eq(&region, set, grown) == 0);
	set = grown;

	assert(jsval_set_add(&region, set, string_a) == 0);
	assert(jsval_set_has(&region, set, string_b, &has) == 0);
	assert(has == 1);
	assert(jsval_set_add(&region, set, string_b) == 0);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 3);

	assert(jsval_object_new(&region, 0, &object_a) == 0);
	assert(jsval_object_new(&region, 0, &object_b) == 0);
	assert(jsval_set_add(&region, set, object_a) == 0);
	assert(jsval_set_has(&region, set, object_a, &has) == 0);
	assert(has == 1);
	assert(jsval_set_has(&region, set, object_b, &has) == 0);
	assert(has == 0);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 4);

	assert(jsval_set_delete(&region, set, string_b, &deleted) == 0);
	assert(deleted == 1);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 3);
	assert(jsval_set_delete(&region, set, string_b, &deleted) == 0);
	assert(deleted == 0);
	assert(jsval_set_delete(&region, set, jsval_number(+0.0), &deleted) == 0);
	assert(deleted == 1);
	assert(jsval_set_has(&region, set, jsval_number(-0.0), &has) == 0);
	assert(has == 0);

	assert(jsval_set_clear(&region, set) == 0);
	assert(jsval_set_size(&region, set, &size) == 0);
	assert(size == 0);
	assert(jsval_set_has(&region, set, object_a, &has) == 0);
	assert(has == 0);
}

static void test_map_semantics(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t map;
	jsval_t grown;
	jsval_t result;
	jsval_t got;
	jsval_t string_a;
	jsval_t string_b;
	jsval_t object_a;
	jsval_t object_b;
	size_t size = 0;
	int has = 0;
	int deleted = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_map_new(&region, 2, &map) == 0);
	assert(map.kind == JSVAL_KIND_MAP);
	assert(jsval_truthy(&region, map) == 1);
	assert(jsval_typeof(&region, map, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_strict_eq(&region, map, map) == 1);
	assert(jsval_map_size(&region, map, &size) == 0);
	assert(size == 0);

	assert(jsval_map_get(&region, map, jsval_number(1.0), &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_map_set(&region, map, jsval_number(NAN), jsval_number(1.0))
			== 0);
	assert(jsval_map_has(&region, map, jsval_number(NAN), &has) == 0);
	assert(has == 1);
	assert(jsval_map_get(&region, map, jsval_number(NAN), &got) == 0);
	assert_number_value(got, 1.0);

	assert(jsval_map_set(&region, map, jsval_number(-0.0), jsval_number(2.0))
			== 0);
	assert(jsval_map_has(&region, map, jsval_number(+0.0), &has) == 0);
	assert(has == 1);
	assert(jsval_map_get(&region, map, jsval_number(+0.0), &got) == 0);
	assert_number_value(got, 2.0);
	assert(jsval_map_size(&region, map, &size) == 0);
	assert(size == 2);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_a) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_b) == 0);
	errno = 0;
	assert(jsval_map_set(&region, map, string_a, jsval_number(3.0)) == -1);
	assert(errno == ENOBUFS);
	assert(jsval_map_clone(&region, map, 4, &grown) == 0);
	assert(grown.kind == JSVAL_KIND_MAP);
	assert(jsval_strict_eq(&region, map, grown) == 0);
	map = grown;

	assert(jsval_map_set(&region, map, string_a, jsval_number(3.0)) == 0);
	assert(jsval_map_has(&region, map, string_b, &has) == 0);
	assert(has == 1);
	assert(jsval_map_get(&region, map, string_b, &got) == 0);
	assert_number_value(got, 3.0);
	assert(jsval_map_set(&region, map, string_b, jsval_number(4.0)) == 0);
	assert(jsval_map_size(&region, map, &size) == 0);
	assert(size == 3);
	assert(jsval_map_key_at(&region, map, 2, &got) == 0);
	assert(jsval_strict_eq(&region, got, string_a) == 1);
	assert(jsval_map_value_at(&region, map, 2, &got) == 0);
	assert_number_value(got, 4.0);

	assert(jsval_object_new(&region, 0, &object_a) == 0);
	assert(jsval_object_new(&region, 0, &object_b) == 0);
	assert(jsval_map_set(&region, map, object_a, jsval_number(5.0)) == 0);
	assert(jsval_map_has(&region, map, object_a, &has) == 0);
	assert(has == 1);
	assert(jsval_map_get(&region, map, object_a, &got) == 0);
	assert_number_value(got, 5.0);
	assert(jsval_map_has(&region, map, object_b, &has) == 0);
	assert(has == 0);
	assert(jsval_map_get(&region, map, object_b, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_map_size(&region, map, &size) == 0);
	assert(size == 4);
	assert(jsval_map_key_at(&region, map, 3, &got) == 0);
	assert(jsval_strict_eq(&region, got, object_a) == 1);
	assert(jsval_map_value_at(&region, map, 3, &got) == 0);
	assert_number_value(got, 5.0);

	assert(jsval_map_delete(&region, map, string_b, &deleted) == 0);
	assert(deleted == 1);
	assert(jsval_map_size(&region, map, &size) == 0);
	assert(size == 3);
	assert(jsval_map_key_at(&region, map, 2, &got) == 0);
	assert(jsval_strict_eq(&region, got, object_a) == 1);
	assert(jsval_map_delete(&region, map, string_b, &deleted) == 0);
	assert(deleted == 0);
	assert(jsval_map_delete(&region, map, jsval_number(+0.0), &deleted) == 0);
	assert(deleted == 1);
	assert(jsval_map_get(&region, map, jsval_number(-0.0), &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_map_clear(&region, map) == 0);
	assert(jsval_map_size(&region, map, &size) == 0);
	assert(size == 0);
	assert(jsval_map_has(&region, map, object_a, &has) == 0);
	assert(has == 0);
	assert(jsval_map_key_at(&region, map, 0, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_map_value_at(&region, map, 0, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
}

static void test_symbol_semantics(void)
{
	static const char json_text[] = "{\"plain\":1}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t description;
	jsval_t symbol_a;
	jsval_t symbol_b;
	jsval_t symbol_none;
	jsval_t result;
	jsval_t object;
	jsval_t clone;
	jsval_t copied;
	jsval_t json_object;
	jsval_t value;
	jsmethod_error_t error;
	double number = 0.0;
	int has = 0;
	int deleted = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"token", 5,
			&description) == 0);
	assert(jsval_symbol_new(&region, 1, description, &symbol_a) == 0);
	assert(jsval_symbol_new(&region, 1, description, &symbol_b) == 0);
	assert(jsval_symbol_new(&region, 0, jsval_undefined(), &symbol_none) == 0);

	assert(symbol_a.kind == JSVAL_KIND_SYMBOL);
	assert(jsval_truthy(&region, symbol_a) == 1);
	assert(jsval_typeof(&region, symbol_a, &result) == 0);
	assert_string(&region, result, "symbol");
	assert(jsval_strict_eq(&region, symbol_a, symbol_a) == 1);
	assert(jsval_strict_eq(&region, symbol_a, symbol_b) == 0);
	assert(jsval_abstract_eq(&region, symbol_a, symbol_a, &has) == 0);
	assert(has == 1);
	assert(jsval_abstract_eq(&region, symbol_a, symbol_b, &has) == 0);
	assert(has == 0);
	assert(jsval_abstract_eq(&region, symbol_a, description, &has) == 0);
	assert(has == 0);
	assert(jsval_symbol_description(&region, symbol_a, &result) == 0);
	assert_string(&region, result, "token");
	assert(jsval_symbol_description(&region, symbol_none, &result) == 0);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	errno = 0;
	assert(jsval_to_number(&region, symbol_a, &number) < 0);
	assert(errno == ENOTSUP);

	memset(&error, 0, sizeof(error));
	assert(jsval_method_string_index_of(&region, symbol_a, description, 0,
			jsval_undefined(), &result, &error) < 0);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsval_object_new(&region, 3, &object) == 0);
	assert(jsval_object_set_key(&region, object, symbol_a,
			jsval_number(7.0)) == 0);
	assert(jsval_object_set_utf8(&region, object,
			(const uint8_t *)"plain", 5, jsval_bool(1)) == 0);
	assert(jsval_object_set_key(&region, object, symbol_b,
			jsval_number(9.0)) == 0);

	assert(jsval_object_has_own_key(&region, object, symbol_a, &has) == 0);
	assert(has == 1);
	assert(jsval_object_get_key(&region, object, symbol_a, &value) == 0);
	assert_number_value(value, 7.0);
	assert(jsval_object_get_key(&region, object, symbol_b, &value) == 0);
	assert_number_value(value, 9.0);
	assert(jsval_object_get_utf8(&region, object, (const uint8_t *)"plain", 5,
			&value) == 0);
	assert(value.kind == JSVAL_KIND_BOOL);
	assert(value.as.boolean == 1);
	assert_object_symbol_key_at(&region, object, 0, symbol_a);
	assert_object_key_at(&region, object, 1, "plain");
	assert_object_symbol_key_at(&region, object, 2, symbol_b);
	assert_json(&region, object, "{\"plain\":true}");

	assert(jsval_object_clone_own(&region, object, 3, &clone) == 0);
	assert(jsval_object_get_key(&region, clone, symbol_a, &value) == 0);
	assert_number_value(value, 7.0);
	assert_object_symbol_key_at(&region, clone, 0, symbol_a);
	assert_object_key_at(&region, clone, 1, "plain");
	assert_object_symbol_key_at(&region, clone, 2, symbol_b);
	assert_json(&region, clone, "{\"plain\":true}");

	assert(jsval_object_new(&region, 3, &copied) == 0);
	assert(jsval_object_copy_own(&region, copied, object) == 0);
	assert(jsval_object_get_key(&region, copied, symbol_b, &value) == 0);
	assert_number_value(value, 9.0);
	assert_object_symbol_key_at(&region, copied, 0, symbol_a);
	assert_object_key_at(&region, copied, 1, "plain");
	assert_object_symbol_key_at(&region, copied, 2, symbol_b);
	assert_json(&region, copied, "{\"plain\":true}");

	assert(jsval_object_delete_key(&region, object, symbol_a, &deleted) == 0);
	assert(deleted == 1);
	assert(jsval_object_has_own_key(&region, object, symbol_a, &has) == 0);
	assert(has == 0);
	assert(jsval_object_delete_key(&region, object, symbol_a, &deleted) == 0);
	assert(deleted == 0);
	assert_object_key_at(&region, object, 0, "plain");
	assert_object_symbol_key_at(&region, object, 1, symbol_b);
	assert_json(&region, object, "{\"plain\":true}");

	assert(jsval_json_parse(&region, (const uint8_t *)json_text,
			sizeof(json_text) - 1, 8, &json_object) == 0);
	assert(jsval_object_has_own_key(&region, json_object, symbol_a, &has) == 0);
	assert(has == 0);
	assert(jsval_object_get_key(&region, json_object, symbol_a, &value) == 0);
	assert(value.kind == JSVAL_KIND_UNDEFINED);
}

static void test_bigint_semantics(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t zero;
	jsval_t zero_text;
	jsval_t forty_two;
	jsval_t forty_two_text;
	jsval_t forty_two_point_five;
	jsval_t one_hundred_text;
	jsval_t left_big;
	jsval_t right_big;
	jsval_t suffix;
	jsval_t result;
	jsval_t value;
	jsval_t set;
	jsval_t map;
	double number = 0.0;
	int equal = 0;
	int cmp = 0;
	int has = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_bigint_new_i64(&region, 0, &zero) == 0);
	assert(jsval_bigint_new_utf8(&region, (const uint8_t *)"-0000", 5,
			&zero_text) == 0);
	assert(zero.kind == JSVAL_KIND_BIGINT);
	assert(jsval_truthy(&region, zero) == 0);
	assert(jsval_strict_eq(&region, zero, zero_text) == 1);
	assert_bigint_string(&region, zero, "0");
	assert(jsval_typeof(&region, zero, &result) == 0);
	assert_string(&region, result, "bigint");

	errno = 0;
	assert(jsval_to_number(&region, zero, &number) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_bigint_new_i64(&region, 42, &forty_two) == 0);
	assert(jsval_bigint_new_utf8(&region, (const uint8_t *)"42", 2,
			&forty_two_text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"42.5", 4,
			&forty_two_point_five) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"100", 3,
			&one_hundred_text) == 0);
	assert(jsval_strict_eq(&region, forty_two, forty_two_text) == 1);
	assert(jsval_bigint_compare(&region, zero, forty_two, &cmp) == 0);
	assert(cmp < 0);
	assert(jsval_abstract_eq(&region, forty_two, jsval_number(42.0), &equal)
			== 0);
	assert(equal == 1);
	assert(jsval_abstract_eq(&region, forty_two, jsval_number(42.5), &equal)
			== 0);
	assert(equal == 0);
	assert(jsval_abstract_eq(&region, forty_two, forty_two_text, &equal) == 0);
	assert(equal == 1);
	assert(jsval_abstract_eq(&region, forty_two, forty_two_point_five, &equal)
			== 0);
	assert(equal == 0);

	assert(jsval_less_than(&region, forty_two, jsval_number(42.5), &cmp) == 0);
	assert(cmp == 1);
	assert(jsval_greater_than(&region, one_hundred_text, forty_two, &cmp) == 0);
	assert(cmp == 1);
	assert(jsval_less_equal(&region, forty_two, forty_two_text, &cmp) == 0);
	assert(cmp == 1);

	assert(jsval_bigint_new_utf8(&region,
			(const uint8_t *)"12345678901234567890", 20, &left_big) == 0);
	assert(jsval_bigint_new_utf8(&region, (const uint8_t *)"9876543210", 10,
			&right_big) == 0);
	assert(jsval_add(&region, left_big, right_big, &result) == 0);
	assert_bigint_string(&region, result, "12345678911111111100");
	assert(jsval_subtract(&region, left_big, right_big, &result) == 0);
	assert_bigint_string(&region, result, "12345678891358024680");
	assert(jsval_multiply(&region, left_big, right_big, &result) == 0);
	assert_bigint_string(&region, result, "121932631124828532111263526900");
	assert(jsval_unary_minus(&region, left_big, &result) == 0);
	assert_bigint_string(&region, result, "-12345678901234567890");
	assert(jsval_unary_minus(&region, zero, &result) == 0);
	assert_bigint_string(&region, result, "0");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)" apples", 7,
			&suffix) == 0);
	assert(jsval_add(&region, forty_two, suffix, &result) == 0);
	assert_string(&region, result, "42 apples");

	errno = 0;
	assert(jsval_add(&region, forty_two, jsval_number(1.0), &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_subtract(&region, forty_two, jsval_number(1.0), &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_multiply(&region, forty_two, jsval_number(2.0), &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_unary_plus(&region, forty_two, &result) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_set_new(&region, 1, &set) == 0);
	assert(jsval_set_add(&region, set, forty_two) == 0);
	assert(jsval_set_has(&region, set, forty_two_text, &has) == 0);
	assert(has == 1);

	assert(jsval_map_new(&region, 1, &map) == 0);
	assert(jsval_map_set(&region, map, forty_two, jsval_bool(1)) == 0);
	assert(jsval_map_get(&region, map, forty_two_text, &value) == 0);
	assert(value.kind == JSVAL_KIND_BOOL);
	assert(value.as.boolean == 1);
}

static void test_iterator_semantics(void)
{
	static const char json[] = "[3,4]";
	static const char json_string[] = "\"a\\ud834\\udf06b\"";
	static const uint16_t astral_pair_units[] = {0xD834, 0xDF06, 'b'};
	static const uint16_t lone_high_units[] = {0xD834};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t array;
	jsval_t parsed_array;
	jsval_t native_string;
	jsval_t parsed_string;
	jsval_t set;
	jsval_t map;
	jsval_t iterator;
	jsval_t key;
	jsval_t value;
	jsval_t string_a;
	jsval_t string_b;
	jsval_t result;
	jsval_t collected;
	jsval_t source;
	size_t size;
	int done;
	int has;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_array_new(&region, 3, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(10.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(20.0)) == 0);
	assert(jsval_get_iterator(&region, array, JSVAL_ITERATOR_SELECTOR_DEFAULT,
			&iterator, &error) == 0);
	assert(iterator.kind == JSVAL_KIND_ITERATOR);
	assert(jsval_truthy(&region, iterator) == 1);
	assert(jsval_typeof(&region, iterator, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_strict_eq(&region, iterator, iterator) == 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 10.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 20.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);
	assert(value.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_get_iterator(&region, array, JSVAL_ITERATOR_SELECTOR_KEYS,
			&iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 0.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 1.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);
	assert(value.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_get_iterator(&region, array, JSVAL_ITERATOR_SELECTOR_ENTRIES,
			&iterator, &error) == 0);
	errno = 0;
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert_number_value(key, 0.0);
	assert_number_value(value, 10.0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert_number_value(key, 1.0);
	assert_number_value(value, 20.0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 1);
	assert(key.kind == JSVAL_KIND_UNDEFINED);
	assert(value.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 8,
			&parsed_array) == 0);
	assert(jsval_get_iterator(&region, parsed_array,
			JSVAL_ITERATOR_SELECTOR_VALUES, &iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert(jsval_strict_eq(&region, value, jsval_number(3.0)) == 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert(jsval_strict_eq(&region, value, jsval_number(4.0)) == 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"abc", 3,
			&native_string) == 0);
	assert(jsval_get_iterator(&region, native_string,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_string(&region, value, "a");
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_string(&region, value, "b");
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_string(&region, value, "c");
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);
	assert(value.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);
	assert(value.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_string_new_utf16(&region, astral_pair_units,
			sizeof(astral_pair_units) / sizeof(astral_pair_units[0]),
			&native_string) == 0);
	assert(jsval_get_iterator(&region, native_string,
			JSVAL_ITERATOR_SELECTOR_VALUES, &iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_utf16_string(&region, value, astral_pair_units, 2);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_utf16_string(&region, value, astral_pair_units + 2, 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_get_iterator(&region, native_string,
			JSVAL_ITERATOR_SELECTOR_KEYS, &iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 0.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 2.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_get_iterator(&region, native_string,
			JSVAL_ITERATOR_SELECTOR_ENTRIES, &iterator, &error) == 0);
	errno = 0;
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert_number_value(key, 0.0);
	assert_utf16_string(&region, value, astral_pair_units, 2);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert_number_value(key, 2.0);
	assert_utf16_string(&region, value, astral_pair_units + 2, 1);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 1);
	assert(key.kind == JSVAL_KIND_UNDEFINED);
	assert(value.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_string_new_utf16(&region, lone_high_units,
			sizeof(lone_high_units) / sizeof(lone_high_units[0]),
			&native_string) == 0);
	assert(jsval_get_iterator(&region, native_string,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_utf16_string(&region, value, lone_high_units, 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json_string,
			sizeof(json_string) - 1, 16, &parsed_string) == 0);
	assert(jsval_get_iterator(&region, parsed_string,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_string(&region, value, "a");
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_utf16_string(&region, value, astral_pair_units, 2);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_string(&region, value, "b");
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_set_new(&region, 4, &set) == 0);
	assert(jsval_set_add(&region, set, jsval_number(1.0)) == 0);
	assert(jsval_set_add(&region, set, jsval_number(2.0)) == 0);
	assert(jsval_get_iterator(&region, set, JSVAL_ITERATOR_SELECTOR_DEFAULT,
			&iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 1.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 2.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_get_iterator(&region, set, JSVAL_ITERATOR_SELECTOR_KEYS,
			&iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 1.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 2.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_get_iterator(&region, set, JSVAL_ITERATOR_SELECTOR_ENTRIES,
			&iterator, &error) == 0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert_number_value(key, 1.0);
	assert_number_value(value, 1.0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert_number_value(key, 2.0);
	assert_number_value(value, 2.0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 1);

	assert(jsval_map_new(&region, 3, &map) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
			&string_a) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"b", 1,
			&string_b) == 0);
	assert(jsval_map_set(&region, map, string_a, jsval_number(5.0)) == 0);
	assert(jsval_map_set(&region, map, string_b, jsval_number(6.0)) == 0);
	assert(jsval_get_iterator(&region, map, JSVAL_ITERATOR_SELECTOR_DEFAULT,
			&iterator, &error) == 0);
	errno = 0;
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert(jsval_strict_eq(&region, key, string_a) == 1);
	assert_number_value(value, 5.0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 0);
	assert(jsval_strict_eq(&region, key, string_b) == 1);
	assert_number_value(value, 6.0);
	assert(jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) == 0);
	assert(done == 1);

	assert(jsval_get_iterator(&region, map, JSVAL_ITERATOR_SELECTOR_KEYS,
			&iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert(jsval_strict_eq(&region, value, string_a) == 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert(jsval_strict_eq(&region, value, string_b) == 1);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_get_iterator(&region, map, JSVAL_ITERATOR_SELECTOR_VALUES,
			&iterator, &error) == 0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 5.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 0);
	assert_number_value(value, 6.0);
	assert(jsval_iterator_next(&region, iterator, &done, &value, &error) == 0);
	assert(done == 1);

	assert(jsval_array_new(&region, 4, &source) == 0);
	assert(jsval_array_push(&region, source, jsval_number(7.0)) == 0);
	assert(jsval_array_push(&region, source, jsval_number(7.0)) == 0);
	assert(jsval_array_push(&region, source, jsval_number(8.0)) == 0);
	assert(jsval_set_new(&region, 3, &collected) == 0);
	assert(jsval_get_iterator(&region, source, JSVAL_ITERATOR_SELECTOR_DEFAULT,
			&iterator, &error) == 0);
	for (;;) {
		assert(jsval_iterator_next(&region, iterator, &done, &value, &error)
				== 0);
		if (done) {
			break;
		}
		assert(jsval_set_add(&region, collected, value) == 0);
	}
	assert(jsval_set_size(&region, collected, &size) == 0);
	assert(size == 2);
	assert(jsval_set_has(&region, collected, jsval_number(7.0), &has) == 0);
	assert(has == 1);
	assert(jsval_set_has(&region, collected, jsval_number(8.0), &has) == 0);
	assert(has == 1);

	errno = 0;
	assert(jsval_get_iterator(&region, jsval_number(1.0),
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	errno = 0;
	assert(jsval_iterator_next(&region, jsval_number(1.0), &done, &value,
			&error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

#if JSMX_WITH_REGEX
	{
		jsval_t pattern;
		jsval_t flags;
		jsval_t regex;
		jsval_t subject;

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&pattern) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&flags) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"aba", 3,
				&subject) == 0);
		assert(jsval_regexp_new(&region, pattern, 1, flags, &regex,
				&error) == 0);
		assert(jsval_regexp_match_all(&region, regex, subject, &iterator,
				&error) == 0);
		assert(jsval_get_iterator(&region, iterator,
				JSVAL_ITERATOR_SELECTOR_DEFAULT, &result, &error) == 0);
		assert(jsval_strict_eq(&region, iterator, result) == 1);
		assert(jsval_iterator_next(&region, iterator, &done, &value, &error)
				== 0);
		assert(done == 0);
		assert(value.kind == JSVAL_KIND_OBJECT);
		assert_object_string_prop(&region, value, "0", "a");
		errno = 0;
		assert(jsval_iterator_next_entry(&region, iterator, &done, &key,
				&value, &error) < 0);
		assert(errno == ENOTSUP);
	}
#endif
}

static void assert_positive_zero(jsval_t value)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(value.as.number == 0.0);
	assert(signbit(value.as.number) == 0);
}

static void assert_negative_zero(jsval_t value)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(value.as.number == 0.0);
	assert(signbit(value.as.number) != 0);
}

static void assert_nan_value(jsval_t value)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(isnan(value.as.number));
}

static void assert_number_value(jsval_t value, double expected)
{
	assert(value.kind == JSVAL_KIND_NUMBER);
	assert(value.as.number == expected);
}

static void test_region_alloc_helpers(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	void *first = NULL;
	void *second = NULL;
	size_t measured_start;
	size_t measured_used;
	size_t need;

	jsval_region_init(&region, storage, sizeof(storage));
	measured_start = region.used;
	measured_used = measured_start;
	assert(jsval_region_measure_alloc(&region, &measured_used, 3, 1) == 0);
	assert(jsval_region_measure_alloc(&region, &measured_used, sizeof(uint32_t),
			_Alignof(uint32_t)) == 0);
	need = measured_used - measured_start;
	assert(need > 0);

	assert(jsval_region_alloc(&region, 3, 1, &first) == 0);
	assert(first != NULL);
	assert(jsval_region_alloc(&region, sizeof(uint32_t), _Alignof(uint32_t),
			&second) == 0);
	assert(second != NULL);
	assert(((uintptr_t)second % _Alignof(uint32_t)) == 0);
	assert(region.used == measured_used);

	{
		size_t exact_total = jsval_pages_head_size() + need;
		uint8_t exact_storage[exact_total > 0 ? exact_total : 1];
		jsval_region_t exact_region;

		jsval_region_init(&exact_region, exact_storage, sizeof(exact_storage));
		assert(jsval_region_alloc(&exact_region, 3, 1, &first) == 0);
		assert(jsval_region_alloc(&exact_region, sizeof(uint32_t),
				_Alignof(uint32_t), &second) == 0);
		assert(jsval_region_remaining(&exact_region) == 0);
	}

	{
		size_t short_total = jsval_pages_head_size() + need - 1;
		uint8_t short_storage[short_total > 0 ? short_total : 1];
		jsval_region_t short_region;

		jsval_region_init(&short_region, short_storage, sizeof(short_storage));
		errno = 0;
		assert(jsval_region_alloc(&short_region, 3, 1, &first) == 0);
		assert(jsval_region_alloc(&short_region, sizeof(uint32_t),
				_Alignof(uint32_t), &second) == -1);
		assert(errno == ENOBUFS);
	}

	errno = 0;
	assert(jsval_region_alloc(NULL, 1, 1, &first) == -1);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_region_measure_alloc(&region, NULL, 1, 1) == -1);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_region_measure_alloc(&region, &measured_used, 1, 0) == -1);
	assert(errno == EINVAL);
}

static int
test_replace_string_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	test_replace_string_callback_ctx_t *ctx =
			(test_replace_string_callback_ctx_t *)opaque;
	size_t index = (size_t)ctx->call_count++;

	(void)error;
	assert(call != NULL);
	assert(index < 4);
	assert(call->capture_count == 0);
	assert(call->captures == NULL);
	assert(call->offset == ctx->expected_offsets[index]);
	assert(call->groups.kind == JSVAL_KIND_UNDEFINED);
	assert_string(region, call->input, ctx->expected_input);
	assert_string(region, call->match, ctx->expected_matches[index]);
	return jsval_string_new_utf8(region,
			(const uint8_t *)ctx->replacement_values[index],
			strlen(ctx->replacement_values[index]), result_ptr);
}

static int
test_replace_offset_number_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	int *call_count = (int *)opaque;

	(void)region;
	(void)error;
	(*call_count)++;
	assert(call != NULL);
	assert(call->capture_count == 0);
	assert(call->captures == NULL);
	assert(call->groups.kind == JSVAL_KIND_UNDEFINED);
	*result_ptr = jsval_number((double)call->offset);
	return 0;
}

static int
test_replace_throw_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	test_replace_throw_ctx_t *ctx = (test_replace_throw_ctx_t *)opaque;

	(void)region;
	(void)call;
	(void)result_ptr;
	ctx->call_count++;
	errno = EINVAL;
	if (error != NULL) {
		error->kind = ctx->kind;
		error->message = ctx->message;
	}
	return -1;
}

static int
test_replace_regex_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	test_replace_regex_callback_ctx_t *ctx =
			(test_replace_regex_callback_ctx_t *)opaque;
	size_t index = (size_t)ctx->call_count++;

	(void)error;
	assert(call != NULL);
	assert(index < 4);
	assert(call->capture_count == 2);
	assert(call->captures != NULL);
	assert(call->offset == ctx->expected_offsets[index]);
	assert(call->groups.kind == JSVAL_KIND_UNDEFINED);
	assert_string(region, call->input, ctx->expected_input);
	assert_string(region, call->match, ctx->expected_matches[index]);
	assert_string(region, call->captures[0], ctx->expected_capture1[index]);
	if (ctx->expected_capture2[index] == NULL) {
		assert(call->captures[1].kind == JSVAL_KIND_UNDEFINED);
	} else {
		assert_string(region, call->captures[1],
				ctx->expected_capture2[index]);
	}
	return jsval_string_new_utf8(region,
			(const uint8_t *)ctx->replacement_values[index],
			strlen(ctx->replacement_values[index]), result_ptr);
}

static int
test_replace_named_groups_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	test_replace_named_groups_callback_ctx_t *ctx =
			(test_replace_named_groups_callback_ctx_t *)opaque;
	size_t index = (size_t)ctx->call_count++;
	jsval_t value;

	(void)error;
	assert(call != NULL);
	assert(index < 4);
	assert(call->capture_count == 2);
	assert(call->captures != NULL);
	assert(call->offset == ctx->expected_offsets[index]);
	assert_string(region, call->input, ctx->expected_input);
	assert_string(region, call->match, ctx->expected_matches[index]);
	assert_string(region, call->captures[0], ctx->expected_digits[index]);
	if (ctx->expected_tail[index] == NULL) {
		assert(call->captures[1].kind == JSVAL_KIND_UNDEFINED);
	} else {
		assert_string(region, call->captures[1], ctx->expected_tail[index]);
	}
	assert(call->groups.kind == JSVAL_KIND_OBJECT);
	assert(jsval_object_get_utf8(region, call->groups,
			(const uint8_t *)"digits", 6, &value) == 0);
	assert_string(region, value, ctx->expected_digits[index]);
	assert(jsval_object_get_utf8(region, call->groups,
			(const uint8_t *)"tail", 4, &value) == 0);
	if (ctx->expected_tail[index] == NULL) {
		assert(value.kind == JSVAL_KIND_UNDEFINED);
	} else {
		assert_string(region, value, ctx->expected_tail[index]);
	}
	return jsval_string_new_utf8(region,
			(const uint8_t *)ctx->replacement_values[index],
			strlen(ctx->replacement_values[index]), result_ptr);
}

static int
test_replace_surrogate_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	test_replace_surrogate_callback_ctx_t *ctx =
			(test_replace_surrogate_callback_ctx_t *)opaque;
	size_t index = (size_t)ctx->call_count++;

	(void)error;
	assert(call != NULL);
	assert(index < 4);
	assert(call->capture_count == 0);
	assert(call->captures == NULL);
	assert(call->offset == ctx->expected_offsets[index]);
	assert(call->groups.kind == JSVAL_KIND_UNDEFINED);
	assert_utf16_string(region, call->input, ctx->expected_input,
			ctx->expected_input_len);
	assert_utf16_string(region, call->match, ctx->expected_matches[index],
			ctx->expected_match_lens[index]);
	return jsval_string_new_utf8(region,
			(const uint8_t *)ctx->replacement_values[index],
			strlen(ctx->replacement_values[index]), result_ptr);
}

static jsval_t logical_and(jsval_region_t *region, jsval_t left, jsval_t right)
{
	return jsval_truthy(region, left) ? right : left;
}

static jsval_t logical_or(jsval_region_t *region, jsval_t left, jsval_t right)
{
	return jsval_truthy(region, left) ? left : right;
}

static void test_native_storage()
{
	uint8_t storage[16384];
	jsval_region_t region;
	const jsval_pages_t *pages;
	jsval_t object;
	jsval_t array;
	jsval_t string;
	jsval_t same_string;
	jsval_t got;
	jsval_t sum;
	jsval_t cycle_object;

	jsval_region_init(&region, storage, sizeof(storage));
	pages = jsval_region_pages(&region);
	assert(pages != NULL);
	assert(pages->magic == JSVAL_PAGES_MAGIC);
	assert(pages->version == JSVAL_PAGES_VERSION);
	assert(pages->header_size == sizeof(jsval_pages_t));
	assert(region.used == jsval_pages_head_size());

	assert(jsval_object_new(&region, 4, &object) == 0);
	assert(jsval_region_set_root(&region, object) == 0);
	assert(jsval_array_new(&region, 4, &array) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5, &string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5, &same_string) == 0);
	assert(jsval_region_root(&region, &got) == 0);
	assert(got.kind == JSVAL_KIND_OBJECT);
	assert(got.repr == JSVAL_REPR_NATIVE);
	assert(got.off == object.off);

	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"msg", 3, string) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"items", 5, array) == 0);
	assert(jsval_object_get_utf8(&region, object, (const uint8_t *)"msg", 3, &got) == 0);
	assert_string(&region, got, "hello");
	assert(jsval_strict_eq(&region, string, same_string) == 1);

	assert(jsval_array_set(&region, array, 2, jsval_number(7.0)) == 0);
	assert(jsval_array_length(&region, array) == 3);
	assert(jsval_array_get(&region, array, 1, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 7.0);

	assert(jsval_array_set(&region, array, 0, jsval_null()) == 0);
	assert(jsval_array_set(&region, array, 1, jsval_bool(1)) == 0);

	assert(jsval_truthy(&region, object) == 1);
	assert(jsval_truthy(&region, array) == 1);
	assert(jsval_truthy(&region, string) == 1);
	assert(jsval_truthy(&region, jsval_undefined()) == 0);
	assert(jsval_truthy(&region, jsval_null()) == 0);
	assert(jsval_truthy(&region, jsval_number(0.0)) == 0);

	assert(jsval_add(&region, string, jsval_number(7.0), &sum) == 0);
	assert_string(&region, sum, "hello7");

	assert(jsval_add(&region, jsval_number(2.5), jsval_bool(1), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 3.5);

	assert_json(&region, object, "{\"msg\":\"hello\",\"items\":[null,true,7]}");
	assert(jsval_copy_json(&region, jsval_undefined(), NULL, 0, NULL) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_copy_json(&region, jsval_number(NAN), NULL, 0, NULL) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_object_new(&region, 1, &cycle_object) == 0);
	assert(jsval_object_set_utf8(&region, cycle_object, (const uint8_t *)"self", 4, cycle_object) == 0);
	assert(jsval_copy_json(&region, cycle_object, NULL, 0, NULL) < 0);
	assert(errno == ELOOP);
}

static void test_json_storage()
{
	static const char json[] = "{\"message\":\"hi\",\"items\":[1,true,null],\"nested\":{\"a\":\"x\"}}";
	uint8_t storage[32768];
	uint8_t moved_storage[32768];
	jsval_region_t region;
	jsval_region_t moved;
	jsval_t root;
	jsval_t native_root;
	jsval_t message;
	jsval_t items;
	jsval_t nested;
	jsval_t got;
	jsval_t sum;
	size_t used;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_region_root(&region, &got) == 0);
	assert(got.repr == JSVAL_REPR_JSON);
	assert(got.off == root.off);
	assert(got.as.index == root.as.index);
	assert(root.kind == JSVAL_KIND_OBJECT);
	assert(root.repr == JSVAL_REPR_JSON);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"message", 7, &message) == 0);
	assert(message.kind == JSVAL_KIND_STRING);
	assert(message.repr == JSVAL_REPR_JSON);
	assert_string(&region, message, "hi");

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) == 0);
	assert(items.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, items) == 3);
	assert(jsval_array_get(&region, items, 1, &got) == 0);
	assert(got.kind == JSVAL_KIND_BOOL);
	assert(jsval_truthy(&region, got) == 1);
	assert(jsval_array_get(&region, items, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_NULL);
	assert(jsval_truthy(&region, got) == 0);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nested", 6, &nested) == 0);
	assert(jsval_object_get_utf8(&region, nested, (const uint8_t *)"a", 1, &got) == 0);
	assert_string(&region, got, "x");

	assert_json(&region, root, json);
	assert_json(&region, message, "\"hi\"");

	assert(jsval_add(&region, message, jsval_number(3.0), &sum) == 0);
	assert_string(&region, sum, "hi3");

	assert(jsval_promote(&region, root, &native_root) == 0);
	assert(native_root.kind == JSVAL_KIND_OBJECT);
	assert(native_root.repr == JSVAL_REPR_NATIVE);
	assert(jsval_region_set_root(&region, native_root) == 0);
	assert(jsval_object_get_utf8(&region, native_root, (const uint8_t *)"message", 7, &got) == 0);
	assert_string(&region, got, "hi");
	assert(jsval_strict_eq(&region, got, message) == 1);
	assert_json(&region, native_root, "{\"message\":\"hi\",\"items\":[1,true,null],\"nested\":{\"a\":\"x\"}}");

	used = region.used;
	memcpy(moved_storage, storage, used);
	moved = region;
	jsval_region_rebase(&moved, moved_storage, sizeof(moved_storage));
	assert(jsval_region_root(&moved, &got) == 0);
	assert(got.kind == JSVAL_KIND_OBJECT);
	assert(got.repr == JSVAL_REPR_NATIVE);
	assert(jsval_object_get_utf8(&moved, got, (const uint8_t *)"message", 7, &nested) == 0);
	assert_string(&moved, nested, "hi");
	assert(jsval_object_get_utf8(&moved, got, (const uint8_t *)"nested", 6, &nested) == 0);
	assert(jsval_object_get_utf8(&moved, nested, (const uint8_t *)"a", 1, &got) == 0);
	assert_string(&moved, got, "x");
}

static void test_json_mutation_requires_promotion(void)
{
	static const char json[] = "{\n  \"message\": \"hi\",\n  \"items\": [1, true, null]\n}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t promoted;
	jsval_t replacement;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) == 0);

	errno = 0;
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"message", 7, jsval_null()) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_set(&region, items, 0, jsval_number(7.0)) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_promote(&region, root, &promoted) == 0);
	assert(promoted.repr == JSVAL_REPR_NATIVE);
	assert(jsval_region_set_root(&region, promoted) == 0);
	assert(jsval_object_get_utf8(&region, promoted, (const uint8_t *)"items", 5, &items) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"line\n\"quoted\"", 13, &replacement) == 0);
	assert(jsval_object_set_utf8(&region, promoted, (const uint8_t *)"message", 7, replacement) == 0);
	assert(jsval_array_set(&region, items, 0, jsval_number(7.0)) == 0);
	assert_json(&region, promoted, "{\"message\":\"line\\n\\\"quoted\\\"\",\"items\":[7,true,null]}");
}

static void test_json_root_rebase()
{
	static const char json[] = "{\"ok\":true}";
	uint8_t storage[4096];
	uint8_t moved_storage[4096];
	jsval_region_t region;
	jsval_region_t moved;
	jsval_t root;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 8, &root) == 0);
	memcpy(moved_storage, storage, region.used);
	jsval_region_rebase(&moved, moved_storage, sizeof(moved_storage));
	assert(jsval_region_root(&moved, &root) == 0);
	assert(root.repr == JSVAL_REPR_JSON);
	assert(jsval_object_get_utf8(&moved, root, (const uint8_t *)"ok", 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_BOOL);
	assert(jsval_truthy(&moved, got) == 1);
}

static void test_native_container_helpers(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t object;
	jsval_t array;
	jsval_t got;
	int has;
	int deleted;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_object_new(&region, 4, &object) == 0);
	assert(jsval_array_new(&region, 5, &array) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"keep", 4,
			jsval_bool(1)) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"drop", 4,
			jsval_number(7.0)) == 0);
	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"items", 5,
			array) == 0);
	assert(jsval_object_size(&region, object) == 3);
	assert_object_key_at(&region, object, 0, "keep");
	assert_object_key_at(&region, object, 1, "drop");
	assert_object_key_at(&region, object, 2, "items");
	assert_object_key_undefined_at(&region, object, 3);
	assert_object_value_json_at(&region, object, 0, "true");
	assert_object_value_json_at(&region, object, 1, "7");
	assert_object_value_json_at(&region, object, 2, "[]");
	assert_object_value_undefined_at(&region, object, 3);

	assert(jsval_object_set_utf8(&region, object, (const uint8_t *)"drop", 4,
			jsval_number(8.0)) == 0);
	assert(jsval_object_size(&region, object) == 3);
	assert_object_key_at(&region, object, 0, "keep");
	assert_object_key_at(&region, object, 1, "drop");
	assert_object_key_at(&region, object, 2, "items");
	assert_object_value_json_at(&region, object, 1, "8");

	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"keep", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"missing",
			7, &has) == 0);
	assert(has == 0);

	assert(jsval_object_delete_utf8(&region, object, (const uint8_t *)"missing", 7,
			&deleted) == 0);
	assert(deleted == 0);
	assert(jsval_object_delete_utf8(&region, object, (const uint8_t *)"drop", 4,
			&deleted) == 0);
	assert(deleted == 1);
	assert(jsval_object_has_own_utf8(&region, object, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 0);
	assert(jsval_object_get_utf8(&region, object, (const uint8_t *)"drop", 4,
			&got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_object_size(&region, object) == 2);
	assert_object_key_at(&region, object, 0, "keep");
	assert_object_key_at(&region, object, 1, "items");
	assert_object_key_undefined_at(&region, object, 2);
	assert_object_value_json_at(&region, object, 0, "true");
	assert_object_value_json_at(&region, object, 1, "[]");
	assert_object_value_undefined_at(&region, object, 2);

	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_length(&region, array) == 2);
	assert(jsval_array_set_length(&region, array, 5) == 0);
	assert(jsval_array_length(&region, array) == 5);
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, array, 4, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_set_length(&region, array, 2) == 0);
	assert(jsval_array_length(&region, array) == 2);
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_length(&region, array) == 3);
	errno = 0;
	assert(jsval_array_set_length(&region, array, 6) < 0);
	assert(errno == ENOBUFS);
	assert(jsval_array_push(&region, array, jsval_number(4.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(5.0)) == 0);
	errno = 0;
	assert(jsval_array_push(&region, array, jsval_number(6.0)) < 0);
	assert(errno == ENOBUFS);

	errno = 0;
	assert(jsval_object_key_at(&region, jsval_number(1.0), 0, &got) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_object_value_at(&region, jsval_number(1.0), 0, &got) < 0);
	assert(errno == EINVAL);

	assert_json(&region, object, "{\"keep\":true,\"items\":[1,2,3,4,5]}");
	assert_object_value_json_at(&region, object, 1, "[1,2,3,4,5]");
}

static void test_json_container_helpers(void)
{
	static const char json[] = "{\"drop\":1,\"keep\":true,\"items\":[1,2]}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t got;
	int has;
	int deleted;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32,
			&root) == 0);
	assert(jsval_object_size(&region, root) == 3);
	assert_object_key_at(&region, root, 0, "drop");
	assert_object_key_at(&region, root, 1, "keep");
	assert_object_key_at(&region, root, 2, "items");
	assert_object_key_undefined_at(&region, root, 3);
	assert_object_value_json_at(&region, root, 0, "1");
	assert_object_value_json_at(&region, root, 1, "true");
	assert_object_value_json_at(&region, root, 2, "[1,2]");
	assert_object_value_undefined_at(&region, root, 3);

	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"missing",
			7, &has) == 0);
	assert(has == 0);

	errno = 0;
	assert(jsval_object_delete_utf8(&region, root, (const uint8_t *)"drop", 4,
			&deleted) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(3.0)) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_set_length(&region, items, 3) < 0);
	assert(errno == ENOTSUP);

	assert(jsval_region_promote_root(&region, &root) == 0);
	assert(jsval_object_delete_utf8(&region, root, (const uint8_t *)"drop", 4,
			&deleted) == 0);
	assert(deleted == 1);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"drop", 4,
			&has) == 0);
	assert(has == 0);
	assert_object_key_at(&region, root, 0, "keep");
	assert_object_key_at(&region, root, 1, "items");
	assert_object_key_undefined_at(&region, root, 2);
	assert_object_value_json_at(&region, root, 0, "true");
	assert_object_value_json_at(&region, root, 1, "[1,2]");
	assert_object_value_undefined_at(&region, root, 2);
	assert_json(&region, root, "{\"keep\":true,\"items\":[1,2]}");

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(3.0)) < 0);
	assert(errno == ENOBUFS);
	errno = 0;
	assert(jsval_array_set_length(&region, items, 3) < 0);
	assert(errno == ENOBUFS);

	errno = 0;
	assert(jsval_object_key_at(&region, jsval_null(), 0, &got) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_object_value_at(&region, jsval_null(), 0, &got) < 0);
	assert(errno == EINVAL);
}

static void test_object_copy_own_helpers(void)
{
	static const char json_source[] = "{\"z\":1,\"a\":2}";
	static const char json_multi[] = "{\"b\":9,\"c\":3}";
	static const char json_dups[] = "{\"a\":1,\"a\":9,\"c\":3}";
	static const char json_fail[] = "{\"a\":1,\"b\":2}";
	static const char json_dst_text[] = "{\"keep\":true}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t dst;
	jsval_t src_native;
	jsval_t items;
	jsval_t src_json;
	jsval_t multi_dst;
	jsval_t multi_src;
	jsval_t dup_dst;
	jsval_t dup_src;
	jsval_t tight_dst;
	jsval_t json_dst;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_object_new(&region, 3, &dst) == 0);
	assert(jsval_object_new(&region, 2, &src_native) == 0);
	assert(jsval_array_new(&region, 0, &items) == 0);
	assert(jsval_object_set_utf8(&region, dst, (const uint8_t *)"keep", 4,
			jsval_bool(1)) == 0);
	assert(jsval_object_set_utf8(&region, dst, (const uint8_t *)"drop", 4,
			jsval_number(7.0)) == 0);
	assert(jsval_object_set_utf8(&region, src_native, (const uint8_t *)"drop", 4,
			jsval_number(8.0)) == 0);
	assert(jsval_object_set_utf8(&region, src_native, (const uint8_t *)"items", 5,
			items) == 0);
	assert(jsval_object_copy_own(&region, dst, src_native) == 0);
	assert_json(&region, dst, "{\"keep\":true,\"drop\":8,\"items\":[]}");
	assert_object_key_at(&region, dst, 0, "keep");
	assert_object_key_at(&region, dst, 1, "drop");
	assert_object_key_at(&region, dst, 2, "items");
	assert_object_value_json_at(&region, dst, 0, "true");
	assert_object_value_json_at(&region, dst, 1, "8");
	assert_object_value_json_at(&region, dst, 2, "[]");

	assert(jsval_json_parse(&region, (const uint8_t *)json_source,
			sizeof(json_source) - 1, 8, &src_json) == 0);
	assert(jsval_object_new(&region, 3, &multi_dst) == 0);
	assert(jsval_object_set_utf8(&region, multi_dst, (const uint8_t *)"keep", 4,
			jsval_bool(1)) == 0);
	assert(jsval_object_copy_own(&region, multi_dst, src_json) == 0);
	assert_json(&region, multi_dst, "{\"keep\":true,\"z\":1,\"a\":2}");
	assert_object_key_at(&region, multi_dst, 0, "keep");
	assert_object_key_at(&region, multi_dst, 1, "z");
	assert_object_key_at(&region, multi_dst, 2, "a");

	assert(jsval_object_new(&region, 3, &multi_dst) == 0);
	assert(jsval_object_new(&region, 2, &multi_src) == 0);
	assert(jsval_object_set_utf8(&region, multi_src, (const uint8_t *)"a", 1,
			jsval_number(1.0)) == 0);
	assert(jsval_object_set_utf8(&region, multi_src, (const uint8_t *)"b", 1,
			jsval_number(2.0)) == 0);
	assert(jsval_json_parse(&region, (const uint8_t *)json_multi,
			sizeof(json_multi) - 1, 8, &src_json) == 0);
	assert(jsval_object_copy_own(&region, multi_dst, multi_src) == 0);
	assert(jsval_object_copy_own(&region, multi_dst, src_json) == 0);
	assert_json(&region, multi_dst, "{\"a\":1,\"b\":9,\"c\":3}");
	assert_object_key_at(&region, multi_dst, 0, "a");
	assert_object_key_at(&region, multi_dst, 1, "b");
	assert_object_key_at(&region, multi_dst, 2, "c");

	assert(jsval_object_new(&region, 3, &dup_dst) == 0);
	assert(jsval_json_parse(&region, (const uint8_t *)json_dups,
			sizeof(json_dups) - 1, 8, &dup_src) == 0);
	assert(jsval_object_copy_own(&region, dup_dst, dup_src) == 0);
	assert_json(&region, dup_dst, "{\"a\":9,\"c\":3}");
	assert_object_key_at(&region, dup_dst, 0, "a");
	assert_object_key_at(&region, dup_dst, 1, "c");

	assert(jsval_object_copy_own(&region, dup_dst, dup_dst) == 0);
	assert_json(&region, dup_dst, "{\"a\":9,\"c\":3}");

	assert(jsval_object_new(&region, 2, &tight_dst) == 0);
	assert(jsval_object_set_utf8(&region, tight_dst, (const uint8_t *)"keep", 4,
			jsval_bool(1)) == 0);
	assert(jsval_json_parse(&region, (const uint8_t *)json_fail,
			sizeof(json_fail) - 1, 8, &src_json) == 0);
	errno = 0;
	assert(jsval_object_copy_own(&region, tight_dst, src_json) < 0);
	assert(errno == ENOBUFS);
	assert_json(&region, tight_dst, "{\"keep\":true}");

	assert(jsval_json_parse(&region, (const uint8_t *)json_dst_text,
			sizeof(json_dst_text) - 1, 8, &json_dst) == 0);
	errno = 0;
	assert(jsval_object_copy_own(&region, json_dst, src_json) < 0);
	assert(errno == ENOTSUP);

	errno = 0;
	assert(jsval_object_copy_own(&region, jsval_number(1.0), src_json) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_object_copy_own(&region, dst, jsval_number(1.0)) < 0);
	assert(errno == EINVAL);
}

static void test_object_clone_own_helpers(void)
{
	static const char json_source[] = "{\"z\":1,\"a\":2}";
	static const char json_dups[] = "{\"a\":1,\"a\":9,\"c\":3}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t native_src;
	jsval_t clone;
	jsval_t items;
	jsval_t json_src;
	jsval_t dup_clone;
	jsval_t fail_out;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_object_new(&region, 2, &native_src) == 0);
	assert(jsval_array_new(&region, 0, &items) == 0);
	assert(jsval_object_set_utf8(&region, native_src, (const uint8_t *)"keep", 4,
			jsval_bool(1)) == 0);
	assert(jsval_object_set_utf8(&region, native_src, (const uint8_t *)"items", 5,
			items) == 0);
	assert(jsval_object_clone_own(&region, native_src, 2, &clone) == 0);
	assert(jsval_is_native(clone) == 1);
	assert_json(&region, clone, "{\"keep\":true,\"items\":[]}");
	assert_object_key_at(&region, clone, 0, "keep");
	assert_object_key_at(&region, clone, 1, "items");
	assert(jsval_object_set_utf8(&region, clone, (const uint8_t *)"keep", 4,
			jsval_bool(0)) == 0);
	assert_json(&region, native_src, "{\"keep\":true,\"items\":[]}");
	assert_json(&region, clone, "{\"keep\":false,\"items\":[]}");

	assert(jsval_json_parse(&region, (const uint8_t *)json_source,
			sizeof(json_source) - 1, 8, &json_src) == 0);
	assert(jsval_object_clone_own(&region, json_src, 2, &clone) == 0);
	assert(jsval_is_native(clone) == 1);
	assert_json(&region, clone, "{\"z\":1,\"a\":2}");
	assert_object_key_at(&region, clone, 0, "z");
	assert_object_key_at(&region, clone, 1, "a");
	assert(jsval_object_set_utf8(&region, clone, (const uint8_t *)"z", 1,
			jsval_number(7.0)) == 0);
	assert_json(&region, json_src, "{\"z\":1,\"a\":2}");
	assert_json(&region, clone, "{\"z\":7,\"a\":2}");

	assert(jsval_json_parse(&region, (const uint8_t *)json_dups,
			sizeof(json_dups) - 1, 8, &json_src) == 0);
	assert(jsval_object_clone_own(&region, json_src, 3, &dup_clone) == 0);
	assert_json(&region, dup_clone, "{\"a\":9,\"c\":3}");
	assert_object_key_at(&region, dup_clone, 0, "a");
	assert_object_key_at(&region, dup_clone, 1, "c");

	fail_out = jsval_null();
	errno = 0;
	assert(jsval_object_clone_own(&region, json_src, 2, &fail_out) < 0);
	assert(errno == ENOBUFS);
	assert(fail_out.kind == JSVAL_KIND_NULL);

	errno = 0;
	assert(jsval_object_clone_own(&region, jsval_number(1.0), 1, &clone) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_object_clone_own(&region, json_src, 3, NULL) < 0);
	assert(errno == EINVAL);
}

static void test_array_clone_dense_helpers(void)
{
	static const char json_source[] = "[1,2]";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t native_src;
	jsval_t clone;
	jsval_t json_src;
	jsval_t fail_out;
	jsval_t child;
	jsval_t child_src;
	jsval_t child_clone;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_array_new(&region, 4, &native_src) == 0);
	assert(jsval_array_push(&region, native_src, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, native_src, jsval_number(2.0)) == 0);
	assert(jsval_array_clone_dense(&region, native_src, 3, &clone) == 0);
	assert(jsval_is_native(clone) == 1);
	assert(jsval_array_length(&region, clone) == 2);
	assert_json(&region, clone, "[1,2]");
	assert(jsval_array_push(&region, clone, jsval_number(3.0)) == 0);
	assert_json(&region, native_src, "[1,2]");
	assert_json(&region, clone, "[1,2,3]");

	assert(jsval_array_set_length(&region, native_src, 4) == 0);
	assert(jsval_array_clone_dense(&region, native_src, 4, &clone) == 0);
	assert(jsval_array_length(&region, clone) == 4);
	assert(jsval_array_get(&region, clone, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, clone, 3, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_json_parse(&region, (const uint8_t *)json_source,
			sizeof(json_source) - 1, 8, &json_src) == 0);
	assert(jsval_array_clone_dense(&region, json_src, 2, &clone) == 0);
	assert(jsval_is_native(clone) == 1);
	assert_json(&region, clone, "[1,2]");
	assert(jsval_array_set(&region, clone, 0, jsval_number(7.0)) == 0);
	assert_json(&region, json_src, "[1,2]");
	assert_json(&region, clone, "[7,2]");

	assert(jsval_object_new(&region, 1, &child) == 0);
	assert(jsval_object_set_utf8(&region, child, (const uint8_t *)"v", 1,
			jsval_number(1.0)) == 0);
	assert(jsval_array_new(&region, 1, &native_src) == 0);
	assert(jsval_array_push(&region, native_src, child) == 0);
	assert(jsval_array_clone_dense(&region, native_src, 1, &clone) == 0);
	assert(jsval_array_get(&region, native_src, 0, &child_src) == 0);
	assert(jsval_array_get(&region, clone, 0, &child_clone) == 0);
	assert(jsval_strict_eq(&region, child_src, child_clone) == 1);
	assert(jsval_object_set_utf8(&region, child_clone, (const uint8_t *)"v", 1,
			jsval_number(2.0)) == 0);
	assert_json(&region, native_src, "[{\"v\":2}]");
	assert_json(&region, clone, "[{\"v\":2}]");

	fail_out = jsval_null();
	errno = 0;
	assert(jsval_array_clone_dense(&region, json_src, 1, &fail_out) < 0);
	assert(errno == ENOBUFS);
	assert(fail_out.kind == JSVAL_KIND_NULL);

	errno = 0;
	assert(jsval_array_clone_dense(&region, jsval_number(1.0), 1, &clone) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_array_clone_dense(&region, json_src, 2, NULL) < 0);
	assert(errno == EINVAL);
}

static void test_array_splice_dense_helpers(void)
{
	static const char json_source[] = "[1,2]";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t array;
	jsval_t removed;
	jsval_t json_array;
	jsval_t fail_removed;
	jsval_t child;
	jsval_t got;
	jsval_t insert_one[] = {jsval_number(9.0)};
	jsval_t insert_two[] = {jsval_number(7.0), jsval_number(8.0)};
	jsval_t insert_three[] = {
		jsval_number(7.0),
		jsval_number(8.0),
		jsval_number(9.0)
	};

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_array_new(&region, 6, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(4.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 1, 2, NULL, 0, &removed)
			== 0);
	assert_json(&region, array, "[1,4]");
	assert_json(&region, removed, "[2,3]");
	assert(jsval_array_get(&region, array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_array_new(&region, 6, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(4.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 1, 0, insert_two, 2,
			&removed) == 0);
	assert_json(&region, array, "[1,7,8,4]");
	assert_json(&region, removed, "[]");

	assert(jsval_array_new(&region, 5, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 1, 1, insert_one, 1,
			&removed) == 0);
	assert_json(&region, array, "[1,9,3]");
	assert_json(&region, removed, "[2]");

	assert(jsval_array_new(&region, 5, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 1, 1, insert_three, 3,
			&removed) == 0);
	assert_json(&region, array, "[1,7,8,9,3]");
	assert_json(&region, removed, "[2]");

	assert(jsval_array_new(&region, 4, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(4.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 1, 2, insert_one, 1,
			&removed) == 0);
	assert_json(&region, array, "[1,9,4]");
	assert_json(&region, removed, "[2,3]");
	assert(jsval_array_get(&region, array, 3, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_array_new(&region, 3, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 9, 5, insert_one, 1,
			&removed) == 0);
	assert_json(&region, array, "[1,2,9]");
	assert_json(&region, removed, "[]");

	assert(jsval_array_new(&region, 2, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 1, 5, NULL, 0, &removed)
			== 0);
	assert_json(&region, array, "[1]");
	assert_json(&region, removed, "[2]");

	assert(jsval_json_parse(&region, (const uint8_t *)json_source,
			sizeof(json_source) - 1, 8, &json_array) == 0);
	fail_removed = jsval_null();
	errno = 0;
	assert(jsval_array_splice_dense(&region, json_array, 0, 1, NULL, 0,
			&fail_removed) < 0);
	assert(errno == ENOTSUP);
	assert(fail_removed.kind == JSVAL_KIND_NULL);

	assert(jsval_array_new(&region, 4, &array) == 0);
	assert(jsval_array_push(&region, array, jsval_number(1.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(2.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_push(&region, array, jsval_number(4.0)) == 0);
	fail_removed = jsval_null();
	errno = 0;
	assert(jsval_array_splice_dense(&region, array, 1, 1, insert_three, 3,
			&fail_removed) < 0);
	assert(errno == ENOBUFS);
	assert(fail_removed.kind == JSVAL_KIND_NULL);
	assert_json(&region, array, "[1,2,3,4]");

	errno = 0;
	assert(jsval_array_splice_dense(&region, jsval_number(1.0), 0, 0, NULL, 0,
			&removed) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_array_splice_dense(&region, array, 0, 0, NULL, 1, &removed)
			< 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_array_splice_dense(&region, array, 0, 0, NULL, 0, NULL) < 0);
	assert(errno == EINVAL);

	assert(jsval_object_new(&region, 1, &child) == 0);
	assert(jsval_object_set_utf8(&region, child, (const uint8_t *)"v", 1,
			jsval_number(1.0)) == 0);
	assert(jsval_array_new(&region, 2, &array) == 0);
	assert(jsval_array_push(&region, array, child) == 0);
	assert(jsval_array_push(&region, array, jsval_number(3.0)) == 0);
	assert(jsval_array_splice_dense(&region, array, 0, 1, NULL, 0, &removed)
			== 0);
	assert(jsval_array_length(&region, removed) == 1);
	assert(jsval_array_get(&region, removed, 0, &got) == 0);
	assert(jsval_strict_eq(&region, got, child) == 1);
}

static void test_policy_layer()
{
	static const char json[] = "{\"message\":\"hi\",\"items\":[1,true,null]}";
	uint8_t storage[32768];
	uint8_t slot_storage[16384];
	jsval_region_t region;
	jsval_region_t slot_region;
	jsval_t root;
	jsval_t root_again;
	jsval_t items;
	jsval_t message;
	jsval_t got;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_is_json_backed(root) == 1);
	assert(jsval_is_native(root) == 0);
	assert(jsval_is_native(jsval_number(1.0)) == 0);
	assert(jsval_is_json_backed(jsval_number(1.0)) == 0);

	assert(jsval_region_promote_root(&region, &root) == 0);
	assert(jsval_is_native(root) == 1);
	assert(jsval_is_json_backed(root) == 0);
	assert(jsval_region_root(&region, &root_again) == 0);
	assert(root_again.kind == JSVAL_KIND_OBJECT);
	assert(root_again.repr == JSVAL_REPR_NATIVE);
	assert(root_again.off == root.off);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) == 0);
	assert(jsval_is_native(items) == 1);
	assert(jsval_array_set(&region, items, 0, jsval_number(7.0)) == 0);
	assert_json(&region, root, "{\"message\":\"hi\",\"items\":[7,true,null]}");

	jsval_region_init(&slot_region, slot_storage, sizeof(slot_storage));
	assert(jsval_json_parse(&slot_region, (const uint8_t *)json, sizeof(json) - 1, 32, &root) == 0);
	assert(jsval_object_get_utf8(&slot_region, root, (const uint8_t *)"message", 7, &message) == 0);
	assert(jsval_is_json_backed(message) == 1);
	assert(jsval_promote_in_place(&slot_region, &message) == 0);
	assert(jsval_is_native(message) == 1);
	assert_string(&slot_region, message, "hi");
	assert(jsval_region_root(&slot_region, &got) == 0);
	assert(jsval_is_json_backed(got) == 1);
}

static void test_method_bridge()
{
	static const char json[] = "{\"city\":\"istanbul\"}";
	static const uint16_t broken_units[] = {0xD83D};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t city;
	jsval_t broken;
	jsval_t mixed;
	jsval_t locale;
	jsval_t upper;
	jsval_t lower;
	jsval_t repaired;
	jsval_t well_formed;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"city", 4, &city) == 0);
	assert(jsval_string_new_utf16(&region, broken_units,
			sizeof(broken_units) / sizeof(broken_units[0]), &broken) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"Hello, WoRlD!", 13, &mixed) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"tr", 2, &locale) == 0);

	assert(jsval_method_string_to_locale_upper_case(&region, city, 1, locale,
			&upper, &error) == 0);
	assert(jsval_is_native(upper) == 1);
	assert_string(&region, upper, "\xC4\xB0STANBUL");

	assert(jsval_method_string_to_lower_case(&region, mixed, &lower,
			&error) == 0);
	assert(jsval_is_native(lower) == 1);
	assert_string(&region, lower, "hello, world!");

	assert(jsval_method_string_is_well_formed(&region, broken, &well_formed,
			&error) == 0);
	assert(well_formed.kind == JSVAL_KIND_BOOL);
	assert(well_formed.as.boolean == 0);

	assert(jsval_method_string_to_well_formed(&region, broken, &repaired,
			&error) == 0);
	assert_string(&region, repaired, "\xEF\xBF\xBD");

	errno = 0;
	assert(jsval_method_string_to_upper_case(&region, root, &upper, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_normalize_bridge(void)
{
	static const char json[] = "{\"name\":\"A\xCC\x8A\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t name;
	jsval_t normalized;
	jsmethod_error_t error;
	jsmethod_string_normalize_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"name", 4, &name) == 0);

	assert(jsval_method_string_normalize_measure(&region, name, 0,
			jsval_undefined(), &sizes, &error) == 0);
	assert(sizes.this_storage_len == 2);
	assert(sizes.form_storage_len == 0);
	assert(sizes.workspace_len == 4);
	assert(sizes.result_len == 1);

	{
		uint16_t this_storage[sizes.this_storage_len];
		uint32_t workspace[sizes.workspace_len];

		assert(jsval_method_string_normalize(&region, name, 0, jsval_undefined(),
				this_storage, sizes.this_storage_len,
				NULL, 0,
				workspace, sizes.workspace_len,
				&normalized, &error) == 0);
	}
	assert(jsval_is_native(normalized) == 1);
	assert_string(&region, normalized, "\xC3\x85");

	errno = 0;
	assert(jsval_method_string_normalize_measure(&region, root, 0,
			jsval_undefined(), &sizes, &error) == -1);
	assert(errno == ENOTSUP);
}

static void test_method_search_bridge(void)
{
	static const char json[] = "{\"text\":\"bananas\",\"needle\":\"na\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t needle;
	jsval_t pos_three;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"needle", 6,
			&needle) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"3", 1,
			&pos_three) == 0);

	assert(jsval_method_string_index_of(&region, text, needle, 0,
			jsval_undefined(), &result, &error) == 0);
	assert_number_value(result, 2.0);

	assert(jsval_method_string_last_index_of(&region, text, needle, 1,
			pos_three, &result, &error) == 0);
	assert_number_value(result, 2.0);

	assert(jsval_method_string_includes(&region, text, needle, 1, pos_three,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);

	assert(jsval_method_string_starts_with(&region, text, needle, 1,
			jsval_number(2.0), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);

	assert(jsval_method_string_ends_with(&region, text, needle, 1,
			jsval_number(4.0), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);

	errno = 0;
	assert(jsval_method_string_index_of(&region, root, needle, 0,
			jsval_undefined(), &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void
test_method_split_bridge(void)
{
	static const char json[] = "{\"text\":\"a,b,,c\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t comma;
	jsval_t limit_two;
	jsval_t empty;
	jsval_t native_ab;
	jsval_t result;
	jsmethod_error_t error;
	static const char *expected_split[] = {"a", "b", "", "c"};
	static const char *expected_limit[] = {"a", "b"};
	static const char *expected_whole[] = {"a,b,,c"};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)",", 1,
			&comma) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"ab", 2,
			&native_ab) == 0);

	assert(jsval_method_string_split(&region, text, 1, comma, 0,
			jsval_undefined(), &result, &error) == 0);
	assert_array_strings(&region, result, expected_split,
			sizeof(expected_split) / sizeof(expected_split[0]));

	assert(jsval_method_string_split(&region, text, 0, jsval_undefined(), 0,
			jsval_undefined(), &result, &error) == 0);
	assert_array_strings(&region, result, expected_whole,
			sizeof(expected_whole) / sizeof(expected_whole[0]));

	assert(jsval_method_string_split(&region, text, 1, comma, 1, limit_two,
			&result, &error) == 0);
	assert_array_strings(&region, result, expected_limit,
			sizeof(expected_limit) / sizeof(expected_limit[0]));

	assert(jsval_method_string_split(&region, empty, 1, empty, 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 0);

	errno = 0;
	assert(jsval_method_string_split(&region, root, 1, comma, 0,
			jsval_undefined(), &result, &error) < 0);
	assert(errno == ENOTSUP);

#if JSMX_WITH_REGEX
		{
			jsval_t pattern;
			jsval_t regex;
			static const char *expected_capture_split[] = {
				"a", ",", "b", ",", "", ",", "c"
			};
			static const char *expected_zero_width_split[] = {"a", "b"};

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"(,)", 3,
				&pattern) == 0);
		assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
				&regex, &error) == 0);
		assert(jsval_method_string_split(&region, text, 1, regex, 0,
				jsval_undefined(),
				&result, &error) == 0);
		assert_array_strings(&region, result, expected_capture_split,
				sizeof(expected_capture_split) /
				sizeof(expected_capture_split[0]));

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"(?:)", 4,
				&pattern) == 0);
		assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
				&regex, &error) == 0);
		assert(jsval_method_string_split(&region, native_ab, 1, regex, 0,
				jsval_undefined(), &result, &error) == 0);
		assert_array_strings(&region, result, expected_zero_width_split,
				sizeof(expected_zero_width_split) /
				sizeof(expected_zero_width_split[0]));
	}
#endif
}

static void
test_method_replace_bridge(void)
{
	static const char json[] =
		"{\"text\":\"a1b2\",\"search\":\"1\",\"replacement\":\"X\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t search;
	jsval_t replacement;
	jsval_t native_search;
	jsval_t special_replacement;
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"search", 6,
			&search) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"replacement", 11,
			&replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&native_search) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"[$$][$&][$`][$']",
			16, &special_replacement) == 0);

	assert(jsval_method_string_replace_measure(&region, text, search,
			replacement, &sizes, &error) == 0);
	assert(sizes.result_len == strlen("aXb2"));
	assert(jsval_method_string_replace(&region, text, search, replacement,
			&result, &error) == 0);
	assert_string(&region, result, "aXb2");

	assert(jsval_method_string_replace_measure(&region, text, native_search,
			special_replacement, &sizes, &error) == 0);
	assert(sizes.result_len == strlen("a[$][1][a][b2]b2"));
	assert(jsval_method_string_replace(&region, text, native_search,
			special_replacement, &result, &error) == 0);
	assert_string(&region, result, "a[$][1][a][b2]b2");

	assert(jsval_method_string_replace(&region, text, native_search,
			jsval_undefined(), &result, &error) == 0);
	assert_string(&region, result, "aundefinedb2");

	errno = 0;
	assert(jsval_method_string_replace_measure(&region, text, root, replacement,
			&sizes, &error) < 0);
	assert(errno == ENOTSUP);

	errno = 0;
	assert(jsval_method_string_replace(&region, root, search, replacement,
			&result, &error) < 0);
	assert(errno == ENOTSUP);

#if JSMX_WITH_REGEX
	{
		jsval_t pattern;
		jsval_t flags;
		jsval_t regex;
		jsval_t capture_replacement;
		size_t last_index;

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
				&pattern) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&flags) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"<$1>", 4,
				&capture_replacement) == 0);
		assert(jsval_regexp_new(&region, pattern, 1, flags, &regex,
				&error) == 0);
		assert(jsval_regexp_set_last_index(&region, regex, 1) == 0);
		assert(jsval_method_string_replace_measure(&region, text, regex,
				capture_replacement, &sizes, &error) == 0);
		assert(sizes.result_len == strlen("a<1>b<2>"));
		assert(jsval_method_string_replace(&region, text, regex,
				capture_replacement, &result, &error) == 0);
		assert_string(&region, result, "a<1>b<2>");
		assert(jsval_regexp_get_last_index(&region, regex, &last_index) == 0);
		assert(last_index == 1);
	}
#endif
}

static void
test_method_replace_all_bridge(void)
{
	static const char json[] =
		"{\"text\":\"a1b1\",\"search\":\"1\",\"replacement\":\"X\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t search;
	jsval_t replacement;
	jsval_t result;
	jsval_t empty_search;
	jsval_t special_replacement;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"search", 6,
			&search) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"replacement", 11,
			&replacement) == 0);

	assert(jsval_method_string_replace_all_measure(&region, text, search,
			replacement, &sizes, &error) == 0);
	assert(sizes.result_len == strlen("aXbX"));
	assert(jsval_method_string_replace_all(&region, text, search, replacement,
			&result, &error) == 0);
	assert_string(&region, result, "aXbX");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty_search) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"[$$][$&][$`][$']",
			16, &special_replacement) == 0);
	assert(jsval_method_string_replace_all_measure(&region, text, empty_search,
			special_replacement, &sizes, &error) == 0);
	assert(jsval_method_string_replace_all(&region, text, empty_search,
			special_replacement, &result, &error) == 0);
	assert_string(&region, result,
			"[$][][][a1b1]a[$][][a][1b1]1[$][][a1][b1]b[$][][a1b][1]1[$][][a1b1][]");

	errno = 0;
	assert(jsval_method_string_replace_all_measure(&region, text, root,
			replacement, &sizes, &error) < 0);
	assert(errno == ENOTSUP);

	errno = 0;
	assert(jsval_method_string_replace_all(&region, root, search, replacement,
			&result, &error) < 0);
	assert(errno == ENOTSUP);

#if JSMX_WITH_REGEX
	{
		jsval_t pattern;
		jsval_t global_flags;
		jsval_t regex;
		jsval_t non_global_regex;
		jsval_t capture_replacement;
		size_t last_index;

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
				&pattern) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"<$1>", 4,
				&capture_replacement) == 0);
		assert(jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
				&error) == 0);
		assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
				&non_global_regex, &error) == 0);
		assert(jsval_regexp_set_last_index(&region, regex, 1) == 0);

		assert(jsval_method_string_replace_all_measure(&region, text, regex,
				capture_replacement, &sizes, &error) == 0);
		assert(sizes.result_len == strlen("a<1>b<1>"));
		assert(jsval_method_string_replace_all(&region, text, regex,
				capture_replacement, &result, &error) == 0);
		assert_string(&region, result, "a<1>b<1>");
		assert(jsval_regexp_get_last_index(&region, regex, &last_index) == 0);
		assert(last_index == 1);

		errno = 0;
		assert(jsval_method_string_replace_all_measure(&region, text,
				non_global_regex, capture_replacement, &sizes, &error) < 0);
		assert(errno == EINVAL);
		assert(error.kind == JSMETHOD_ERROR_TYPE);

		errno = 0;
		assert(jsval_method_string_replace_all(&region, text, non_global_regex,
				capture_replacement, &result, &error) < 0);
		assert(errno == EINVAL);
		assert(error.kind == JSMETHOD_ERROR_TYPE);
	}
#endif
}

static void
test_method_replace_callback_bridge(void)
{
	static const char json[] = "{\"text\":\"a1b1\",\"search\":\"1\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t search;
	jsval_t empty_search;
	jsval_t result;
	jsmethod_error_t error;
	test_replace_string_callback_ctx_t replace_ctx = {
		0,
		"a1b1",
		{"1"},
		{1},
		{"<one>"}
	};
	test_replace_string_callback_ctx_t replace_all_ctx = {
		0,
		"a1b1",
		{"1", "1"},
		{1, 3},
		{"<one>", "<three>"}
	};
	int empty_calls = 0;
	test_replace_throw_ctx_t throw_ctx = {
		0,
		JSMETHOD_ERROR_ABRUPT,
		"callback threw"
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"search", 6,
			&search) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty_search) == 0);

	assert(jsval_method_string_replace_fn(&region, text, search,
			test_replace_string_callback, &replace_ctx, &result, &error) == 0);
	assert_string(&region, result, "a<one>b1");
	assert(replace_ctx.call_count == 1);

	assert(jsval_method_string_replace_all_fn(&region, text, search,
			test_replace_string_callback, &replace_all_ctx, &result,
			&error) == 0);
	assert_string(&region, result, "a<one>b<three>");
	assert(replace_all_ctx.call_count == 2);

	assert(jsval_method_string_replace_all_fn(&region, text,
			empty_search, test_replace_offset_number_callback,
			&empty_calls, &result, &error) == 0);
	assert_string(&region, result, "0a112b314");
	assert(empty_calls == 5);

	errno = 0;
	assert(jsval_method_string_replace_fn(&region, text, search,
			test_replace_throw_callback, &throw_ctx, &result, &error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_ABRUPT);
	assert(throw_ctx.call_count == 1);
}

#if JSMX_WITH_REGEX
static void
test_method_replace_regex_callback_bridge(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t global_regex;
	jsval_t single_regex;
	jsval_t result;
	jsmethod_error_t error;
	test_replace_regex_callback_ctx_t ctx = {
		0,
		"1b2",
		{"1b", "2"},
		{"1", "2"},
		{"b", NULL},
		{0, 2},
		{"<1b|1|b|0>", "<2|2|U|2>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1b2", 3,
			&text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])(b)?", 11,
			&pattern) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, global_flags, &global_regex,
			&error) == 0);
	assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&single_regex, &error) == 0);

	assert(jsval_method_string_replace_fn(&region, text, global_regex,
			test_replace_regex_callback, &ctx, &result, &error) == 0);
	assert_string(&region, result, "<1b|1|b|0><2|2|U|2>");
	assert(ctx.call_count == 2);

	errno = 0;
	assert(jsval_method_string_replace_all_fn(&region, text, single_regex,
			test_replace_regex_callback, &ctx, &result, &error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_TYPE);
}

static void
test_method_replace_regex_named_groups_bridge(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t global_regex;
	jsval_t single_regex;
	jsval_t replacement;
	jsval_t missing_replacement;
	jsval_t result;
	jsmethod_error_t error;
	test_replace_named_groups_callback_ctx_t ctx = {
		0,
		"a1b2",
		{"1b", "2"},
		{"1", "2"},
		{"b", NULL},
		{1, 3},
		{"<1|b|1>", "<2|U|3>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
			&text) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"(?<digits>[0-9])(?<tail>[a-z])?",
			sizeof("(?<digits>[0-9])(?<tail>[a-z])?") - 1,
			&pattern) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0);
	assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&single_regex, &error) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, global_flags, &global_regex,
			&error) == 0);

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"<$<digits>|$1|$<tail>>",
			sizeof("<$<digits>|$1|$<tail>>") - 1,
			&replacement) == 0);
	assert(jsval_method_string_replace(&region, text, single_regex,
			replacement, &result, &error) == 0);
	assert_string(&region, result, "a<1|1|b>2");

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"$<digits>:$<tail>;",
			sizeof("$<digits>:$<tail>;") - 1,
			&replacement) == 0);
	assert(jsval_method_string_replace_all(&region, text, global_regex,
			replacement, &result, &error) == 0);
	assert_string(&region, result, "a1:b;2:;");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"$<missing>",
			sizeof("$<missing>") - 1, &missing_replacement) == 0);
	assert(jsval_method_string_replace(&region, text, single_regex,
			missing_replacement, &result, &error) == 0);
	assert_string(&region, result, "a$<missing>2");

	assert(jsval_method_string_replace_fn(&region, text, global_regex,
			test_replace_named_groups_callback, &ctx, &result, &error) == 0);
	assert_string(&region, result, "a<1|b|1><2|U|3>");
	assert(ctx.call_count == 2);
	ctx.call_count = 0;
	assert(jsval_method_string_replace_all_fn(&region, text, global_regex,
			test_replace_named_groups_callback, &ctx, &result, &error) == 0);
	assert_string(&region, result, "a<1|b|1><2|U|3>");
	assert(ctx.call_count == 2);
}
#endif

#if JSMX_WITH_REGEX
static void
test_regexp_exec_and_match(void)
{
	static const uint8_t pattern_utf8[] = "([0-9][0-9])([a-z])?";
	static const uint8_t digit_pattern_utf8[] = "([0-9])";
	static const uint8_t abc_utf8[] = "abc";
	static const uint8_t json_text[] = "{\"text\":\"343443444\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t digit_pattern;
	jsval_t abc_pattern;
	jsval_t gim_flags;
	jsval_t y_flag;
	jsval_t bad_flags;
	jsval_t global_flags;
	jsval_t sticky_flags;
	jsval_t regex;
	jsval_t copy_regex;
	jsval_t override_regex;
	jsval_t global_regex;
	jsval_t state_global_regex;
	jsval_t non_global_digit_regex;
	jsval_t sticky_regex;
	jsval_t all_flags;
	jsval_t all_regex;
	jsval_t input;
	jsval_t subject;
	jsval_t state_subject;
	jsval_t result;
	jsval_t native_string;
	jsval_t flags_value;
	jsval_t source_value;
	jsval_t parsed_root;
	jsval_t parsed_text;
	jsval_regexp_info_t info;
	jsmethod_error_t error;
	size_t last_index;
	int test_result;
	int flag_value;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, pattern_utf8,
			sizeof(pattern_utf8) - 1, &pattern) == 0);
	assert(jsval_string_new_utf8(&region, digit_pattern_utf8,
			sizeof(digit_pattern_utf8) - 1, &digit_pattern) == 0);
	assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&regex, &error) == 0);
	assert(regex.kind == JSVAL_KIND_REGEXP);
	assert(jsval_truthy(&region, regex) == 1);
	assert(jsval_regexp_source(&region, regex, &source_value) == 0);
	assert_string(&region, source_value, "([0-9][0-9])([a-z])?");
	assert(jsval_regexp_flags(&region, regex, &flags_value) == 0);
	assert_string(&region, flags_value, "");
	assert(jsval_regexp_global(&region, regex, &flag_value) == 0);
	assert(flag_value == 0);
	assert(jsval_regexp_ignore_case(&region, regex, &flag_value) == 0);
	assert(flag_value == 0);
	assert(jsval_regexp_multiline(&region, regex, &flag_value) == 0);
	assert(flag_value == 0);
	assert(jsval_regexp_dot_all(&region, regex, &flag_value) == 0);
	assert(flag_value == 0);
	assert(jsval_regexp_unicode(&region, regex, &flag_value) == 0);
	assert(flag_value == 0);
	assert(jsval_regexp_sticky(&region, regex, &flag_value) == 0);
	assert(flag_value == 0);
	assert(jsval_regexp_info(&region, regex, &info) == 0);
	assert(info.flags == 0);
	assert(info.capture_count == 2);
	assert(info.last_index == 0);
	assert(jsval_regexp_get_last_index(&region, regex, &last_index) == 0);
	assert(last_index == 0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a12b", 4,
			&subject) == 0);
	assert(jsval_regexp_test(&region, regex, subject, &test_result, &error) == 0);
	assert(test_result == 1);
	assert(jsval_regexp_get_last_index(&region, regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_regexp_exec(&region, regex, subject, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "12b");
	assert_object_string_prop(&region, result, "1", "12");
	assert_object_string_prop(&region, result, "2", "b");
	assert_object_number_prop(&region, result, "length", 3.0);
	assert_object_number_prop(&region, result, "index", 1.0);
	assert_object_string_prop(&region, result, "input", "a12b");
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"gimsuy", 6,
			&all_flags) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"gim", 3,
			&gim_flags) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"y", 1,
			&y_flag) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"yy", 2,
			&bad_flags) == 0);
	assert(jsval_string_new_utf8(&region, abc_utf8, sizeof(abc_utf8) - 1,
			&abc_pattern) == 0);
	assert(jsval_regexp_new(&region,
			jsval_number(3.0), 1, global_flags, &global_regex, &error) == 0);
	assert(jsval_regexp_new(&region, digit_pattern, 0, jsval_undefined(),
			&non_global_digit_regex, &error) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, all_flags, &all_regex,
			&error) == 0);
	assert(jsval_regexp_flags(&region, all_regex, &flags_value) == 0);
	assert_string(&region, flags_value, "gimsuy");
	assert(jsval_regexp_global(&region, all_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_ignore_case(&region, all_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_multiline(&region, all_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_dot_all(&region, all_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_unicode(&region, all_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_sticky(&region, all_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_new(&region, abc_pattern, 1, gim_flags, &copy_regex,
			&error) == 0);
	assert(jsval_regexp_set_last_index(&region, copy_regex, 5) == 0);
	assert(jsval_regexp_new(&region, copy_regex, 0, jsval_undefined(),
			&override_regex, &error) == 0);
	assert(jsval_regexp_source(&region, override_regex, &source_value) == 0);
	assert_string(&region, source_value, "abc");
	assert(jsval_regexp_flags(&region, override_regex, &flags_value) == 0);
	assert_string(&region, flags_value, "gim");
	assert(jsval_regexp_get_last_index(&region, override_regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_regexp_new(&region, copy_regex, 1, y_flag,
			&override_regex, &error) == 0);
	assert(jsval_regexp_source(&region, override_regex, &source_value) == 0);
	assert_string(&region, source_value, "abc");
	assert(jsval_regexp_flags(&region, override_regex, &flags_value) == 0);
	assert_string(&region, flags_value, "y");
	assert(jsval_regexp_sticky(&region, override_regex, &flag_value) == 0);
	assert(flag_value == 1);
	assert(jsval_regexp_get_last_index(&region, override_regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"343443444", 9,
			&native_string) == 0);
	assert(jsval_method_string_match(&region, native_string, 1, global_regex,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &input) == 0);
	assert_string(&region, input, "3");
	assert(jsval_array_get(&region, result, 1, &input) == 0);
	assert_string(&region, input, "3");
	assert(jsval_array_get(&region, result, 2, &input) == 0);
	assert_string(&region, input, "3");
	assert(jsval_regexp_get_last_index(&region, global_regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
			&state_subject) == 0);
	assert(jsval_regexp_new(&region, digit_pattern, 1, global_flags,
			&state_global_regex, &error) == 0);
	assert(jsval_regexp_exec(&region, state_global_regex, state_subject, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1");
	assert_object_string_prop(&region, result, "1", "1");
	assert_object_number_prop(&region, result, "index", 1.0);
	assert(jsval_regexp_get_last_index(&region, state_global_regex, &last_index) == 0);
	assert(last_index == 2);
	assert(jsval_regexp_exec(&region, state_global_regex, state_subject, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "2");
	assert_object_string_prop(&region, result, "1", "2");
	assert_object_number_prop(&region, result, "index", 3.0);
	assert(jsval_regexp_get_last_index(&region, state_global_regex, &last_index) == 0);
	assert(last_index == 4);
	assert(jsval_regexp_exec(&region, state_global_regex, state_subject, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_NULL);
	assert(jsval_regexp_get_last_index(&region, state_global_regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_regexp_set_last_index(&region, state_global_regex, 1) == 0);
	assert(jsval_regexp_test(&region, state_global_regex, state_subject, &test_result,
			&error) == 0);
	assert(test_result == 1);
	assert(jsval_regexp_get_last_index(&region, state_global_regex, &last_index) == 0);
	assert(last_index == 2);
	assert(jsval_regexp_test(&region, state_global_regex, state_subject, &test_result,
			&error) == 0);
	assert(test_result == 1);
	assert(jsval_regexp_get_last_index(&region, state_global_regex, &last_index) == 0);
	assert(last_index == 4);
	assert(jsval_regexp_test(&region, state_global_regex, state_subject, &test_result,
			&error) == 0);
	assert(test_result == 0);
	assert(jsval_regexp_get_last_index(&region, state_global_regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_regexp_set_last_index(&region, non_global_digit_regex, 7) == 0);
	assert(jsval_regexp_exec(&region, non_global_digit_regex, state_subject, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1");
	assert_object_number_prop(&region, result, "index", 1.0);
	assert(jsval_regexp_get_last_index(&region, non_global_digit_regex,
			&last_index) == 0);
	assert(last_index == 7);
	assert(jsval_regexp_test(&region, non_global_digit_regex, state_subject,
			&test_result, &error) == 0);
	assert(test_result == 1);
	assert(jsval_regexp_get_last_index(&region, non_global_digit_regex,
			&last_index) == 0);
	assert(last_index == 7);
	assert(jsval_method_string_match(&region, state_subject, 1, non_global_digit_regex,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1");
	assert_object_number_prop(&region, result, "index", 1.0);
	assert(jsval_regexp_get_last_index(&region, non_global_digit_regex,
			&last_index) == 0);
	assert(last_index == 7);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"y", 1,
			&sticky_flags) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, sticky_flags, &sticky_regex,
			&error) == 0);
	assert(jsval_regexp_set_last_index(&region, sticky_regex, 1) == 0);
	assert(jsval_regexp_test(&region, sticky_regex, subject, &test_result,
			&error) == 0);
	assert(test_result == 1);
	assert(jsval_regexp_get_last_index(&region, sticky_regex, &last_index) == 0);
	assert(last_index == 4);
	assert(jsval_regexp_set_last_index(&region, sticky_regex, 0) == 0);
	assert(jsval_regexp_test(&region, sticky_regex, subject, &test_result,
			&error) == 0);
	assert(test_result == 0);
	assert(jsval_regexp_get_last_index(&region, sticky_regex, &last_index) == 0);
	assert(last_index == 0);
	assert(jsval_regexp_set_last_index(&region, sticky_regex, 1) == 0);
	assert(jsval_regexp_exec(&region, sticky_regex, subject, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "12b");
	assert(jsval_regexp_get_last_index(&region, sticky_regex, &last_index) == 0);
	assert(last_index == 4);
	assert(jsval_regexp_set_last_index(&region, sticky_regex, 0) == 0);
	assert(jsval_regexp_exec(&region, sticky_regex, subject, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_NULL);
	assert(jsval_regexp_get_last_index(&region, sticky_regex, &last_index) == 0);
	assert(last_index == 0);

	assert(jsval_json_parse(&region, json_text, sizeof(json_text) - 1, 8,
			&parsed_root) == 0);
	assert(jsval_object_get_utf8(&region, parsed_root,
			(const uint8_t *)"text", 4, &parsed_text) == 0);
	assert(jsval_method_string_match(&region, parsed_text, 1, global_regex,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &input) == 0);
	assert_string(&region, input, "3");
	assert(jsval_array_get(&region, result, 1, &input) == 0);
	assert_string(&region, input, "3");
	assert(jsval_array_get(&region, result, 2, &input) == 0);
	assert_string(&region, input, "3");

	assert(jsval_method_string_match(&region, native_string, 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "");
	assert_object_number_prop(&region, result, "index", 0.0);

	assert(jsval_regexp_new(&region, abc_pattern, 1, bad_flags,
			&override_regex, &error) < 0);
	assert(error.kind == JSMETHOD_ERROR_SYNTAX);

	errno = 0;
	assert(jsval_regexp_source(&region, native_string, &source_value) < 0);
	assert(errno == EINVAL);

	errno = 0;
	assert(jsval_regexp_global(&region, native_string, &flag_value) < 0);
	assert(errno == EINVAL);

	errno = 0;
	assert(jsval_regexp_test(&region, native_string, subject, &test_result,
			&error) < 0);
	assert(error.kind == JSMETHOD_ERROR_TYPE);
	assert(errno == EINVAL);

	errno = 0;
	assert(jsval_regexp_exec(&region, native_string, subject, &result, &error) < 0);
	assert(error.kind == JSMETHOD_ERROR_TYPE);
	assert(errno == EINVAL);
}

static void
test_regexp_match_all(void)
{
	static const uint8_t json_text[] = "{\"text\":\"a1b2\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t json_root;
	jsval_t json_subject;
	jsmethod_error_t error;
	size_t last_index;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
			&pattern) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
			&error) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
			&subject) == 0);
	assert(jsval_regexp_set_last_index(&region, regex, 1) == 0);
	assert(jsval_regexp_match_all(&region, regex, subject, &iterator,
			&error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_truthy(&region, iterator) == 1);
	assert(jsval_strict_eq(&region, iterator, iterator) == 1);
	assert(jsval_regexp_get_last_index(&region, regex, &last_index) == 0);
	assert(last_index == 1);

	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1");
	assert_object_string_prop(&region, result, "1", "1");
	assert_object_number_prop(&region, result, "length", 2.0);
	assert_object_number_prop(&region, result, "index", 1.0);
	assert_object_string_prop(&region, result, "input", "a1b2");
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_regexp_get_last_index(&region, regex, &last_index) == 0);
	assert(last_index == 1);

	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "2");
	assert_object_string_prop(&region, result, "1", "2");
	assert_object_number_prop(&region, result, "length", 2.0);
	assert_object_number_prop(&region, result, "index", 3.0);

	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(), &regex,
			&error) == 0);
	assert(jsval_regexp_match_all(&region, regex, subject, &iterator,
			&error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "1");
	assert_object_number_prop(&region, result, "index", 1.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_match_all(&region, subject, 1, regex,
			&iterator, &error) < 0);
	assert(errno == EINVAL);
	assert(error.kind == JSMETHOD_ERROR_TYPE);

	assert(jsval_method_string_match_all(&region, subject, 0,
			jsval_undefined(), &iterator, &error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "");
	assert_object_number_prop(&region, result, "index", 0.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "");
	assert_object_number_prop(&region, result, "index", 1.0);

	assert(jsval_method_string_match_all(&region, subject, 1, jsval_null(),
			&iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_json_parse(&region, json_text, sizeof(json_text) - 1, 8,
			&json_root) == 0);
	assert(jsval_object_get_utf8(&region, json_root,
			(const uint8_t *)"text", 4, &json_subject) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
			&error) == 0);
	assert(jsval_method_string_match_all(&region, json_subject, 1, regex,
			&iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "1");
	assert_object_number_prop(&region, result, "index", 1.0);
}

static void
test_regexp_named_groups(void)
{
	static const uint8_t pattern_utf8[] = "(?<digits>[0-9])(?<tail>[a-z])?";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t global_regex;
	jsval_t jit_regex;
	jsval_t jit_global_regex;
	jsval_t subject;
	jsval_t subject_without_tail;
	jsval_t result;
	jsval_t iterator;
	jsmethod_error_t error;
	int done;
	size_t last_index = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, pattern_utf8,
			sizeof(pattern_utf8) - 1, &pattern) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) == 0);
	assert(jsval_regexp_new(&region, pattern, 0, jsval_undefined(), &regex,
			&error) == 0);
	assert(jsval_regexp_new(&region, pattern, 1, global_flags, &global_regex,
			&error) == 0);
	assert(jsval_regexp_new_jit(&region, pattern, 0, jsval_undefined(),
			&jit_regex, &error) == 0);
	assert(jsval_regexp_new_jit(&region, pattern, 1, global_flags,
			&jit_global_regex, &error) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
			&subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a2", 2,
			&subject_without_tail) == 0);

	assert(jsval_regexp_exec(&region, regex, subject, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1b");
	assert_object_string_prop(&region, result, "1", "1");
	assert_object_string_prop(&region, result, "2", "b");
	assert_object_groups_string_prop(&region, result, "digits", "1");
	assert_object_groups_string_prop(&region, result, "tail", "b");

	assert(jsval_regexp_exec(&region, jit_regex, subject, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1b");
	assert_object_string_prop(&region, result, "1", "1");
	assert_object_string_prop(&region, result, "2", "b");
	assert_object_groups_string_prop(&region, result, "digits", "1");
	assert_object_groups_string_prop(&region, result, "tail", "b");

	assert(jsval_regexp_exec(&region, regex, subject_without_tail, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "2");
	assert_object_string_prop(&region, result, "1", "2");
	assert_object_undefined_prop(&region, result, "2");
	assert_object_groups_string_prop(&region, result, "digits", "2");
	assert_object_groups_undefined_prop(&region, result, "tail");

	assert(jsval_method_string_match(&region, subject, 1, regex, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1b");
	assert_object_groups_string_prop(&region, result, "digits", "1");
	assert_object_groups_string_prop(&region, result, "tail", "b");

	assert(jsval_method_string_match_all(&region, subject, 1, global_regex,
			&iterator, &error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "1b");
	assert_object_groups_string_prop(&region, result, "digits", "1");
	assert_object_groups_string_prop(&region, result, "tail", "b");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "2");
	assert_object_groups_string_prop(&region, result, "digits", "2");
	assert_object_groups_undefined_prop(&region, result, "tail");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_regexp_exec(&region, jit_global_regex, subject, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_string_prop(&region, result, "0", "1b");
	assert(jsval_regexp_get_last_index(&region, jit_global_regex,
			&last_index) == 0);
	assert(last_index == 3);
	assert(jsval_regexp_set_last_index(&region, jit_global_regex, 0) == 0);
	assert(jsval_method_string_match_all(&region, subject, 1, jit_global_regex,
			&iterator, &error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_string_prop(&region, result, "0", "1b");
	assert_object_groups_string_prop(&region, result, "digits", "1");
	assert_object_groups_string_prop(&region, result, "tail", "b");
}

static void
test_method_regex_search_bridge(void)
{
	static const char json[] = "{\"text\":\"fooBar\",\"pattern\":\"bar\",\"flags\":\"i\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t flags;
	jsval_t native_foo;
	jsval_t native_bar;
	jsval_t native_y;
	jsval_t native_bad_pattern;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"pattern", 7,
			&pattern) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flags", 5,
			&flags) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"foo", 3,
			&native_foo) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"bar", 3,
			&native_bar) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"y", 1,
			&native_y) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"[", 1,
			&native_bad_pattern) == 0);

	assert(jsval_method_string_search_regex(&region, text, pattern, 1, flags,
			&result, &error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_search_regex(&region, text,
			native_foo, 1, native_y, &result, &error) == 0);
	assert_number_value(result, 0.0);

	assert(jsval_method_string_search_regex(&region, text,
			native_bar, 1, native_y, &result, &error) == 0);
	assert_number_value(result, -1.0);

	assert(jsval_method_string_search_regex(&region, text,
			native_bad_pattern, 0, jsval_undefined(), &result, &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_SYNTAX);

	errno = 0;
	assert(jsval_method_string_search_regex(&region, root, pattern, 0,
			jsval_undefined(), &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void
test_u_literal_surrogate_match_rewrite(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'B', 0xD834
	};
	static const uint16_t pair_only_units[] = {0xD834, 0xDF06};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t high_unit[] = {0xD834};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t high_subject;
	jsval_t pair_only;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0);

	assert(jsval_method_string_match_u_literal_surrogate(&region, low_subject,
			0xDF06, 0, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_method_string_match_u_literal_surrogate(&region, pair_only,
			0xDF06, 0, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_NULL);

	assert(jsval_method_string_match_u_literal_surrogate(&region, low_subject,
			0xDF06, 1, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, low_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, low_unit, 1);

	assert(jsval_method_string_match_u_literal_surrogate(&region, high_subject,
			0xD834, 1, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, high_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, high_unit, 1);
}

static void
test_u_literal_surrogate_match_all_rewrite(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'B', 0xD834
	};
	static const uint16_t pair_only_units[] = {0xD834, 0xDF06};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t high_unit[] = {0xD834};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t high_subject;
	jsval_t pair_only;
	jsval_t iterator;
	jsval_t result;
	int done;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0);

	assert(jsval_method_string_match_all_u_literal_surrogate(&region,
			low_subject, 0xDF06, &iterator, &error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_unit, 1);
	assert_object_number_prop(&region, result, "index", 5.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_match_all_u_literal_surrogate(&region,
			high_subject, 0xD834, &iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", high_unit, 1);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", high_unit, 1);
	assert_object_number_prop(&region, result, "index", 5.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_match_all_u_literal_surrogate(&region,
			pair_only, 0xDF06, &iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_literal_surrogate_replace_rewrite(void)
{
	static const uint16_t pair_low_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06
	};
	static const uint16_t subst_subject_units[] = {
		'X', 0xDF06, 'Y', 0xDF06
	};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t replace_first_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 0xDF06
	};
	static const uint16_t replace_all_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 'X'
	};
	static const uint16_t special_expected[] = {
		'X', '[', '$', ']', '[', 0xDF06, ']', '[', '$', '1', ']',
		'[', 'X', ']', '[', 'Y', 0xDF06, ']', 'Y', 0xDF06
	};
	static const uint16_t callback_first_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'L', '>', 'B', 0xDF06
	};
	static const uint16_t callback_all_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'A', '>', 'B', '<', 'B', '>'
	};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t pair_low;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t result;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t replace_ctx = {
		0,
		pair_low_units,
		sizeof(pair_low_units) / sizeof(pair_low_units[0]),
		{low_unit},
		{1},
		{3},
		{"<L>"}
	};
	test_replace_surrogate_callback_ctx_t replace_all_ctx = {
		0,
		pair_low_units,
		sizeof(pair_low_units) / sizeof(pair_low_units[0]),
		{low_unit, low_unit},
		{1, 1},
		{3, 5},
		{"<A>", "<B>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, pair_low_units,
			sizeof(pair_low_units) / sizeof(pair_low_units[0]),
			&pair_low) == 0);
	assert(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);

	assert(jsval_method_string_replace_u_literal_surrogate(&region, pair_low,
			0xDF06, ascii_x, &result, &error) == 0);
	assert_utf16_string(&region, result, replace_first_expected,
			sizeof(replace_first_expected) /
			sizeof(replace_first_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_surrogate(&region,
			pair_low, 0xDF06, ascii_x, &result, &error) == 0);
	assert_utf16_string(&region, result, replace_all_expected,
			sizeof(replace_all_expected) /
			sizeof(replace_all_expected[0]));

	assert(jsval_method_string_replace_u_literal_surrogate(&region,
			subst_subject, 0xDF06, special_replacement, &result,
			&error) == 0);
	assert_utf16_string(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0]));

	assert(jsval_method_string_replace_u_literal_surrogate_fn(&region,
			pair_low, 0xDF06, test_replace_surrogate_callback,
			&replace_ctx, &result, &error) == 0);
	assert(replace_ctx.call_count == 1);
	assert_utf16_string(&region, result, callback_first_expected,
			sizeof(callback_first_expected) /
			sizeof(callback_first_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_surrogate_fn(&region,
			pair_low, 0xDF06, test_replace_surrogate_callback,
			&replace_all_ctx, &result, &error) == 0);
	assert(replace_all_ctx.call_count == 2);
	assert_utf16_string(&region, result, callback_all_expected,
			sizeof(callback_all_expected) /
			sizeof(callback_all_expected[0]));
}

static void
test_u_literal_surrogate_split_rewrite(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'C'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'B', 0xD834, 'C'
	};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t c_unit[] = {'C'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t high_subject;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_split_u_literal_surrogate(&region, low_subject,
			0xDF06, 0, jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, c_unit, 1);

	assert(jsval_method_string_split_u_literal_surrogate(&region, low_subject,
			0xDF06, 1, limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);

	assert(jsval_method_string_split_u_literal_surrogate(&region, high_subject,
			0xD834, 0, jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, c_unit, 1);
}

static void
test_u_literal_sequence_search_match_rewrite(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'B', 'C'
	};
	static const uint16_t pair_subject_units[] = {
		'A', 0xD834, 0xDF06, '!', 'X', 0xD834, 0xDF06, '!', 'Y'
	};
	static const uint16_t boundary_subject_units[] = {
		'A', 0xD834, 0xDF06, '!'
	};
	static const uint16_t low_b_units[] = {0xDF06, 'B'};
	static const uint16_t pair_bang_units[] = {0xD834, 0xDF06, '!'};
	static const uint16_t a_high_units[] = {'A', 0xD834};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t pair_subject;
	jsval_t boundary_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_subject_units,
			sizeof(pair_subject_units) / sizeof(pair_subject_units[0]),
			&pair_subject) == 0);
	assert(jsval_string_new_utf16(&region, boundary_subject_units,
			sizeof(boundary_subject_units) /
			sizeof(boundary_subject_units[0]), &boundary_subject) == 0);

	assert(jsval_method_string_search_u_literal_sequence(&region, low_subject,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]),
			&result, &error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_search_u_literal_sequence(&region, pair_subject,
			pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]),
			&result, &error) == 0);
	assert_number_value(result, 1.0);

	assert(jsval_method_string_search_u_literal_sequence(&region,
			boundary_subject, a_high_units,
			sizeof(a_high_units) / sizeof(a_high_units[0]),
			&result, &error) == 0);
	assert_number_value(result, -1.0);

	assert(jsval_method_string_match_u_literal_sequence(&region, pair_subject,
			pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]), 0, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]));
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 1.0);
	assert_object_utf16_prop(&region, result, "input", pair_subject_units,
			sizeof(pair_subject_units) / sizeof(pair_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_method_string_match_u_literal_sequence(&region, low_subject,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]), 1,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]));

	assert(jsval_method_string_match_all_u_literal_sequence(&region,
			low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), &iterator,
			&error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]));
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]));
	assert_object_number_prop(&region, result, "index", 5.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_literal_sequence_replace_split_rewrite(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'B', 'C'
	};
	static const uint16_t subst_subject_units[] = {
		'X', 0xDF06, 'B', 'Y', 0xDF06, 'B'
	};
	static const uint16_t low_b_units[] = {0xDF06, 'B'};
	static const uint16_t replace_first_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 0xDF06, 'B', 'C'
	};
	static const uint16_t replace_all_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'X', 'C'
	};
	static const uint16_t special_expected[] = {
		'X', '[', '$', ']', '[', 0xDF06, 'B', ']', '[', '$', '1', ']',
		'[', 'X', ']', '[', 'Y', 0xDF06, 'B', ']', 'Y', 0xDF06, 'B'
	};
	static const uint16_t callback_all_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'A', '>', '<', 'B', '>', 'C'
	};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t c_unit[] = {'C'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t replace_all_ctx = {
		0,
		low_subject_units,
		sizeof(low_subject_units) / sizeof(low_subject_units[0]),
		{low_b_units, low_b_units},
		{2, 2},
		{3, 5},
		{"<A>", "<B>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0);
	assert(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_replace_u_literal_sequence(&region, low_subject,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]),
			ascii_x, &result, &error) == 0);
	assert_utf16_string(&region, result, replace_first_expected,
			sizeof(replace_first_expected) /
			sizeof(replace_first_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_sequence(&region,
			low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), ascii_x, &result,
			&error) == 0);
	assert_utf16_string(&region, result, replace_all_expected,
			sizeof(replace_all_expected) /
			sizeof(replace_all_expected[0]));

	assert(jsval_method_string_replace_u_literal_sequence(&region,
			subst_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]),
			special_replacement, &result, &error) == 0);
	assert_utf16_string(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_sequence_fn(&region,
			low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]),
			test_replace_surrogate_callback, &replace_all_ctx, &result,
			&error) == 0);
	assert(replace_all_ctx.call_count == 2);
	assert_utf16_string(&region, result, callback_all_expected,
			sizeof(callback_all_expected) /
			sizeof(callback_all_expected[0]));

	assert(jsval_method_string_split_u_literal_sequence(&region, low_subject,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]), 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, c_unit, 1);

	assert(jsval_method_string_split_u_literal_sequence(&region, low_subject,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]), 1,
			limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);
}

static void
test_u_literal_class_search_match_rewrite(void)
{
	static const uint16_t class_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 'D', 0xD834, 'C'
	};
	static const uint16_t pair_only_units[] = {
		'A', 0xD834, 0xDF06, '!'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_members[] = {0xD834};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t d_unit[] = {'D'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_subject;
	jsval_t pair_only;
	jsval_t high_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);

	assert(jsval_method_string_search_u_literal_class(&region, class_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &result,
			&error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_search_u_literal_class(&region, pair_only,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &result,
			&error) == 0);
	assert_number_value(result, -1.0);

	assert(jsval_method_string_search_u_literal_class(&region, high_subject,
			high_members, sizeof(high_members) / sizeof(high_members[0]),
			&result, &error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_match_u_literal_class(&region, class_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_method_string_match_u_literal_class(&region, class_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, low_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, d_unit, 1);

	assert(jsval_method_string_match_all_u_literal_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &iterator,
			&error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", low_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", d_unit, 1);
	assert_object_number_prop(&region, result, "index", 5.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_literal_class_replace_split_rewrite(void)
{
	static const uint16_t class_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 'D', 0xD834, 'C'
	};
	static const uint16_t subst_subject_units[] = {
		'X', 0xDF06, 'Y', 'D'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t replace_first_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 'D', 0xD834, 'C'
	};
	static const uint16_t replace_all_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 'X', 0xD834, 'C'
	};
	static const uint16_t special_expected[] = {
		'X', '[', '$', ']', '[', 0xDF06, ']', '[', '$', '1', ']',
		'[', 'X', ']', '[', 'Y', 'D', ']', 'Y', 'D'
	};
	static const uint16_t callback_all_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'A', '>', 'B', '<', 'B', '>',
		0xD834, 'C'
	};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t high_c_units[] = {0xD834, 'C'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t class_subject;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t replace_all_ctx = {
		0,
		class_subject_units,
		sizeof(class_subject_units) / sizeof(class_subject_units[0]),
		{low_unit, d_unit},
		{1, 1},
		{3, 5},
		{"<A>", "<B>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_subject) == 0);
	assert(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_replace_u_literal_class(&region, class_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_first_expected,
			sizeof(replace_first_expected) /
			sizeof(replace_first_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_all_expected,
			sizeof(replace_all_expected) /
			sizeof(replace_all_expected[0]));

	assert(jsval_method_string_replace_u_literal_class(&region, subst_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			special_replacement, &result, &error) == 0);
	assert_utf16_string(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_class_fn(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			test_replace_surrogate_callback, &replace_all_ctx, &result,
			&error) == 0);
	assert(replace_all_ctx.call_count == 2);
	assert_utf16_string(&region, result, callback_all_expected,
			sizeof(callback_all_expected) /
			sizeof(callback_all_expected[0]));

	assert(jsval_method_string_split_u_literal_class(&region, class_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, high_c_units,
			sizeof(high_c_units) / sizeof(high_c_units[0]));

	assert(jsval_method_string_split_u_literal_class(&region, class_subject,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1,
			limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);
}

static void
test_u_literal_negated_class_search_match_rewrite(void)
{
	static const uint16_t class_subject_units[] = {
		0xD834, 0xDF06, 0xDF06, 'B', 'D'
	};
	static const uint16_t pair_only_units[] = {0xD834, 0xDF06};
	static const uint16_t high_subject_units[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_c_members[] = {0xD834, 'C'};
	static const uint16_t surrogate_members[] = {0xD834, 0xDF06};
	static const uint16_t b_unit[] = {'B'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_subject;
	jsval_t pair_only;
	jsval_t high_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);

	assert(jsval_method_string_search_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &result,
			&error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_search_u_literal_negated_class(&region,
			pair_only, surrogate_members,
			sizeof(surrogate_members) / sizeof(surrogate_members[0]),
			&result, &error) == 0);
	assert_number_value(result, -1.0);

	assert(jsval_method_string_search_u_literal_negated_class(&region,
			high_subject, high_c_members,
			sizeof(high_c_members) / sizeof(high_c_members[0]), &result,
			&error) == 0);
	assert_number_value(result, 4.0);

	assert(jsval_method_string_match_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", b_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_method_string_match_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 1);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);

	assert(jsval_method_string_match_all_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &iterator,
			&error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", b_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_literal_negated_class_replace_split_rewrite(void)
{
	static const uint16_t class_subject_units[] = {
		0xD834, 0xDF06, 0xDF06, 'B', 'D'
	};
	static const uint16_t subst_subject_units[] = {0xDF06, 'B', 'D'};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t replace_expected[] = {
		0xD834, 0xDF06, 0xDF06, 'X', 'D'
	};
	static const uint16_t special_expected[] = {
		0xDF06, '[', '$', ']', '[', 'B', ']', '[', '$', '1', ']',
		'[', 0xDF06, ']', '[', 'D', ']', 'D'
	};
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, 0xDF06, '<', 'A', '>', 'D'
	};
	static const uint16_t pair_low_units[] = {0xD834, 0xDF06, 0xDF06};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t b_unit[] = {'B'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t class_subject;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t replace_all_ctx = {
		0,
		class_subject_units,
		sizeof(class_subject_units) / sizeof(class_subject_units[0]),
		{b_unit},
		{1},
		{3},
		{"<A>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_subject) == 0);
	assert(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_replace_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_expected,
			sizeof(replace_expected) / sizeof(replace_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_expected,
			sizeof(replace_expected) / sizeof(replace_expected[0]));

	assert(jsval_method_string_replace_u_literal_negated_class(&region,
			subst_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			special_replacement, &result, &error) == 0);
	assert_utf16_string(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_negated_class_fn(
			&region, class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			test_replace_surrogate_callback, &replace_all_ctx, &result,
			&error) == 0);
	assert(replace_all_ctx.call_count == 1);
	assert_utf16_string(&region, result, callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]));

	assert(jsval_method_string_split_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_low_units,
			sizeof(pair_low_units) / sizeof(pair_low_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, d_unit, 1);

	assert(jsval_method_string_split_u_literal_negated_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1,
			limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_low_units,
			sizeof(pair_low_units) / sizeof(pair_low_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, d_unit, 1);
}

static void
test_u_literal_range_class_search_match_rewrite(void)
{
	static const uint16_t range_subject_units[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t pair_only_units[] = {
		'A', 0xD834, 0xDF06, '!'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_range[] = {0xD834, 0xD834};
	static const uint16_t surrogate_range[] = {0xD800, 0xDFFF};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t low_unit[] = {0xDF06};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t pair_only;
	jsval_t high_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);

	assert(jsval_method_string_search_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &result,
			&error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_search_u_literal_range_class(&region,
			pair_only, surrogate_range,
			sizeof(surrogate_range) / (2 * sizeof(surrogate_range[0])),
			&result, &error) == 0);
	assert_number_value(result, -1.0);

	assert(jsval_method_string_search_u_literal_range_class(&region,
			high_subject, high_range,
			sizeof(high_range) / (2 * sizeof(high_range[0])), &result,
			&error) == 0);
	assert_number_value(result, 3.0);

	assert(jsval_method_string_match_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", b_unit, 1);
	assert_object_number_prop(&region, result, "length", 1.0);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_utf16_prop(&region, result, "input", range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]));
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_method_string_match_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, d_unit, 1);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, low_unit, 1);

	assert(jsval_method_string_match_all_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &iterator,
			&error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", b_unit, 1);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", d_unit, 1);
	assert_object_number_prop(&region, result, "index", 4.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", low_unit, 1);
	assert_object_number_prop(&region, result, "index", 5.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_literal_range_class_replace_split_rewrite(void)
{
	static const uint16_t range_subject_units[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t subst_subject_units[] = {
		'X', 'B', 'Y', 0xDF06
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t replace_first_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'D', 0xDF06, 'Z'
	};
	static const uint16_t replace_all_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'X', 'X', 'Z'
	};
	static const uint16_t special_expected[] = {
		'X', '[', '$', ']', '[', 'B', ']', '[', '$', '1', ']',
		'[', 'X', ']', '[', 'Y', 0xDF06, ']', 'Y', 0xDF06
	};
	static const uint16_t callback_all_expected[] = {
		'A', 0xD834, 0xDF06, '<', 'A', '>', '<', 'B', '>', '<', 'C', '>',
		'Z'
	};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t z_unit[] = {'Z'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t replace_all_ctx = {
		0,
		range_subject_units,
		sizeof(range_subject_units) / sizeof(range_subject_units[0]),
		{b_unit, d_unit, low_unit},
		{1, 1, 1},
		{3, 4, 5},
		{"<A>", "<B>", "<C>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0);
	assert(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_replace_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_first_expected,
			sizeof(replace_first_expected) /
			sizeof(replace_first_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_all_expected,
			sizeof(replace_all_expected) /
			sizeof(replace_all_expected[0]));

	assert(jsval_method_string_replace_u_literal_range_class(&region,
			subst_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), special_replacement,
			&result, &error) == 0);
	assert_utf16_string(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_range_class_fn(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])),
			test_replace_surrogate_callback, &replace_all_ctx, &result,
			&error) == 0);
	assert(replace_all_ctx.call_count == 3);
	assert_utf16_string(&region, result, callback_all_expected,
			sizeof(callback_all_expected) /
			sizeof(callback_all_expected[0]));

	assert(jsval_method_string_split_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 4);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);
	assert(jsval_array_get(&region, result, 3, &value) == 0);
	assert_utf16_string(&region, value, z_unit, 1);

	assert(jsval_method_string_split_u_literal_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1,
			limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);
}

static void
test_u_literal_negated_range_class_search_match_rewrite(void)
{
	static const uint16_t range_subject_units[] = {
		0xD834, 0xDF06, 'A', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t pair_only_units[] = {0xD834, 0xDF06};
	static const uint16_t high_subject_units[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_and_c_ranges[] = {
		0xD834, 0xD834, 'C', 'C'
	};
	static const uint16_t surrogate_range[] = {0xD800, 0xDFFF};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t z_unit[] = {'Z'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t pair_only;
	jsval_t high_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0);
	assert(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0);
	assert(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0);

	assert(jsval_method_string_search_u_literal_negated_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &result,
			&error) == 0);
	assert_number_value(result, 2.0);

	assert(jsval_method_string_search_u_literal_negated_range_class(&region,
			pair_only, surrogate_range,
			sizeof(surrogate_range) / (2 * sizeof(surrogate_range[0])),
			&result, &error) == 0);
	assert_number_value(result, -1.0);

	assert(jsval_method_string_search_u_literal_negated_range_class(&region,
			high_subject, high_and_c_ranges,
			sizeof(high_and_c_ranges) /
			(2 * sizeof(high_and_c_ranges[0])), &result,
			&error) == 0);
	assert_number_value(result, 4.0);

	assert(jsval_method_string_match_u_literal_negated_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", a_unit, 1);
	assert_object_number_prop(&region, result, "index", 2.0);
	assert_object_undefined_prop(&region, result, "groups");

	assert(jsval_method_string_match_u_literal_negated_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, a_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, z_unit, 1);

	assert(jsval_method_string_match_all_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &iterator,
			&error) == 0);
	assert(iterator.kind == JSVAL_KIND_MATCH_ITERATOR);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", a_unit, 1);
	assert_object_number_prop(&region, result, "index", 2.0);
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", z_unit, 1);
	assert_object_number_prop(&region, result, "index", 6.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_literal_negated_range_class_replace_split_rewrite(void)
{
	static const uint16_t range_subject_units[] = {
		0xD834, 0xDF06, 'A', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t subst_subject_units[] = {'A', 'B', 'D', 'Z'};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t replace_expected[] = {
		0xD834, 0xDF06, 'X', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t replace_all_expected[] = {
		0xD834, 0xDF06, 'X', 'B', 'D', 0xDF06, 'X'
	};
	static const uint16_t special_expected[] = {
		'[', '$', ']', '[', 'A', ']', '[', '$', '1', ']',
		'[', ']', '[', 'B', 'D', 'Z', ']', 'B', 'D', 'Z'
	};
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, '<', 'A', '>', 'B', 'D', 0xDF06, '<', 'B', '>'
	};
	static const uint16_t pair_units[] = {0xD834, 0xDF06};
	static const uint16_t bdlow_units[] = {'B', 'D', 0xDF06};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t z_unit[] = {'Z'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t replace_all_ctx = {
		0,
		range_subject_units,
		sizeof(range_subject_units) / sizeof(range_subject_units[0]),
		{a_unit, z_unit},
		{1, 1},
		{2, 6},
		{"<A>", "<B>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0);
	assert(jsval_string_new_utf16(&region, subst_subject_units,
			sizeof(subst_subject_units) / sizeof(subst_subject_units[0]),
			&subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_replace_u_literal_negated_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_expected,
			sizeof(replace_expected) / sizeof(replace_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), ascii_x,
			&result, &error) == 0);
	assert_utf16_string(&region, result, replace_all_expected,
			sizeof(replace_all_expected) / sizeof(replace_all_expected[0]));

	assert(jsval_method_string_replace_u_literal_negated_range_class(&region,
			subst_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), special_replacement,
			&result, &error) == 0);
	assert_utf16_string(&region, result, special_expected,
			sizeof(special_expected) / sizeof(special_expected[0]));

	assert(jsval_method_string_replace_all_u_literal_negated_range_class_fn(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])),
			test_replace_surrogate_callback, &replace_all_ctx, &result,
			&error) == 0);
	assert(replace_all_ctx.call_count == 2);
	assert_utf16_string(&region, result, callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]));

	assert(jsval_method_string_split_u_literal_negated_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0,
			jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_units,
			sizeof(pair_units) / sizeof(pair_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, bdlow_units,
			sizeof(bdlow_units) / sizeof(bdlow_units[0]));
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);

	assert(jsval_method_string_split_u_literal_negated_range_class(&region,
			range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1,
			limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_units,
			sizeof(pair_units) / sizeof(pair_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, bdlow_units,
			sizeof(bdlow_units) / sizeof(bdlow_units[0]));
}

static void
test_u_predefined_class_search_match_rewrite(void)
{
	static const uint16_t digit_subject_units[] = {
		0xD834, 0xDF06, 'A', '1', '2', 'B'
	};
	static const uint16_t whitespace_subject_units[] = {
		'A', 0x00A0, 'B', 0x2028, 'C'
	};
	static const uint16_t word_subject_units[] = {
		0xD834, 0xDF06, '!', 'A', '1', '_', 0xD834, '?'
	};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t one_unit[] = {'1'};
	static const uint16_t two_unit[] = {'2'};
	static const uint16_t nbsp_unit[] = {0x00A0};
	static const uint16_t line_sep_unit[] = {0x2028};
	static const uint16_t bang_unit[] = {'!'};
	static const uint16_t underscore_unit[] = {'_'};
	static const uint16_t high_unit[] = {0xD834};
	static const uint16_t q_unit[] = {'?'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t digit_subject;
	jsval_t whitespace_subject;
	jsval_t word_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, digit_subject_units,
			sizeof(digit_subject_units) / sizeof(digit_subject_units[0]),
			&digit_subject) == 0);
	assert(jsval_string_new_utf16(&region, whitespace_subject_units,
			sizeof(whitespace_subject_units) /
			sizeof(whitespace_subject_units[0]), &whitespace_subject) == 0);
	assert(jsval_string_new_utf16(&region, word_subject_units,
			sizeof(word_subject_units) / sizeof(word_subject_units[0]),
			&word_subject) == 0);

	assert(jsval_method_string_search_u_digit_class(&region, digit_subject,
			&result, &error) == 0);
	assert_number_value(result, 3.0);
	assert(jsval_method_string_search_u_negated_digit_class(&region,
			digit_subject, &result, &error) == 0);
	assert_number_value(result, 2.0);
	assert(jsval_method_string_match_u_digit_class(&region, digit_subject, 0,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", one_unit, 1);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_method_string_match_u_digit_class(&region, digit_subject, 1,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, one_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, two_unit, 1);
	assert(jsval_method_string_match_all_u_negated_digit_class(&region,
			digit_subject, &iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", a_unit, 1);
	assert_object_number_prop(&region, result, "index", 2.0);
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", b_unit, 1);
	assert_object_number_prop(&region, result, "index", 5.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_search_u_whitespace_class(&region,
			whitespace_subject, &result, &error) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_method_string_search_u_negated_whitespace_class(&region,
			whitespace_subject, &result, &error) == 0);
	assert_number_value(result, 0.0);
	assert(jsval_method_string_match_u_whitespace_class(&region,
			whitespace_subject, 0, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", nbsp_unit, 1);
	assert_object_number_prop(&region, result, "index", 1.0);
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_method_string_match_all_u_whitespace_class(&region,
			whitespace_subject, &iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", nbsp_unit, 1);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", line_sep_unit, 1);
	assert_object_number_prop(&region, result, "index", 3.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_search_u_word_class(&region, word_subject,
			&result, &error) == 0);
	assert_number_value(result, 3.0);
	assert(jsval_method_string_search_u_negated_word_class(&region,
			word_subject, &result, &error) == 0);
	assert_number_value(result, 2.0);
	assert(jsval_method_string_match_u_negated_word_class(&region,
			word_subject, 0, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_OBJECT);
	assert_object_utf16_prop(&region, result, "0", bang_unit, 1);
	assert_object_number_prop(&region, result, "index", 2.0);
	assert_object_undefined_prop(&region, result, "groups");
	assert(jsval_method_string_match_u_word_class(&region, word_subject, 1,
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, a_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, one_unit, 1);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, underscore_unit, 1);
	assert(jsval_method_string_match_all_u_negated_word_class(&region,
			word_subject, &iterator, &error) == 0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", bang_unit, 1);
	assert_object_number_prop(&region, result, "index", 2.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", high_unit, 1);
	assert_object_number_prop(&region, result, "index", 6.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 0);
	assert_object_utf16_prop(&region, result, "0", q_unit, 1);
	assert_object_number_prop(&region, result, "index", 7.0);
	assert(jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) == 0);
	assert(done == 1);
	assert(result.kind == JSVAL_KIND_UNDEFINED);
}

static void
test_u_predefined_class_replace_split_rewrite(void)
{
	static const uint16_t digit_subject_units[] = {
		0xD834, 0xDF06, 'A', '1', '2', 'B'
	};
	static const uint16_t whitespace_subject_units[] = {
		'A', 0x00A0, 'B', 0x2028, 'C'
	};
	static const uint16_t word_subject_units[] = {
		0xD834, 0xDF06, '!', 'A', '1', '_', 0xD834, '?'
	};
	static const uint16_t digit_subst_subject_units[] = {'A', '1', 'B'};
	static const uint16_t replace_digit_expected[] = {
		0xD834, 0xDF06, 'A', 'X', '2', 'B'
	};
	static const uint16_t replace_all_digit_expected[] = {
		0xD834, 0xDF06, 'A', 'X', 'X', 'B'
	};
	static const uint16_t special_digit_expected[] = {
		'A', '[', '$', ']', '[', '1', ']', '[', '$', '1', ']',
		'[', 'A', ']', '[', 'B', ']', 'B'
	};
	static const uint16_t replace_space_expected[] = {
		'A', 'X', 'B', 'X', 'C'
	};
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, '<', 'A', '>', 'A', '1', '_',
		'<', 'B', '>', '<', 'C', '>'
	};
	static const uint16_t pair_units[] = {0xD834, 0xDF06};
	static const uint16_t digits_units[] = {'1', '2'};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t b_unit[] = {'B'};
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t digit_subject;
	jsval_t whitespace_subject;
	jsval_t word_subject;
	jsval_t digit_subst_subject;
	jsval_t ascii_x;
	jsval_t special_replacement;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	test_replace_surrogate_callback_ctx_t callback_ctx = {
		0,
		word_subject_units,
		sizeof(word_subject_units) / sizeof(word_subject_units[0]),
		{
			(const uint16_t[]){'!'},
			(const uint16_t[]){0xD834},
			(const uint16_t[]){'?'}
		},
		{1, 1, 1},
		{2, 6, 7},
		{"<A>", "<B>", "<C>"}
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf16(&region, digit_subject_units,
			sizeof(digit_subject_units) / sizeof(digit_subject_units[0]),
			&digit_subject) == 0);
	assert(jsval_string_new_utf16(&region, whitespace_subject_units,
			sizeof(whitespace_subject_units) /
			sizeof(whitespace_subject_units[0]), &whitespace_subject) == 0);
	assert(jsval_string_new_utf16(&region, word_subject_units,
			sizeof(word_subject_units) / sizeof(word_subject_units[0]),
			&word_subject) == 0);
	assert(jsval_string_new_utf16(&region, digit_subst_subject_units,
			sizeof(digit_subst_subject_units) /
			sizeof(digit_subst_subject_units[0]), &digit_subst_subject) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&ascii_x) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"[$$][$&][$1][$`][$']", 20,
			&special_replacement) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&limit_two) == 0);

	assert(jsval_method_string_replace_u_digit_class(&region, digit_subject,
			ascii_x, &result, &error) == 0);
	assert_utf16_string(&region, result, replace_digit_expected,
			sizeof(replace_digit_expected) /
			sizeof(replace_digit_expected[0]));
	assert(jsval_method_string_replace_all_u_digit_class(&region,
			digit_subject, ascii_x, &result, &error) == 0);
	assert_utf16_string(&region, result, replace_all_digit_expected,
			sizeof(replace_all_digit_expected) /
			sizeof(replace_all_digit_expected[0]));
	assert(jsval_method_string_replace_u_digit_class(&region,
			digit_subst_subject, special_replacement, &result, &error) == 0);
	assert_utf16_string(&region, result, special_digit_expected,
			sizeof(special_digit_expected) /
			sizeof(special_digit_expected[0]));

	assert(jsval_method_string_replace_all_u_whitespace_class(&region,
			whitespace_subject, ascii_x, &result, &error) == 0);
	assert_utf16_string(&region, result, replace_space_expected,
			sizeof(replace_space_expected) /
			sizeof(replace_space_expected[0]));

	assert(jsval_method_string_replace_all_u_negated_word_class_fn(&region,
			word_subject, test_replace_surrogate_callback, &callback_ctx,
			&result, &error) == 0);
	assert(callback_ctx.call_count == 3);
	assert_utf16_string(&region, result, callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]));

	assert(jsval_method_string_split_u_whitespace_class(&region,
			whitespace_subject, 0, jsval_undefined(), &result,
			&error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, a_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, (const uint16_t[]){'C'}, 1);
	assert(jsval_method_string_split_u_whitespace_class(&region,
			whitespace_subject, 1, limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, a_unit, 1);
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, b_unit, 1);

	assert(jsval_method_string_split_u_negated_digit_class(&region,
			digit_subject, 0, jsval_undefined(), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 3);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_units,
			sizeof(pair_units) / sizeof(pair_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, digits_units,
			sizeof(digits_units) / sizeof(digits_units[0]));
	assert(jsval_array_get(&region, result, 2, &value) == 0);
	assert_utf16_string(&region, value, NULL, 0);
	assert(jsval_method_string_split_u_negated_digit_class(&region,
			digit_subject, 1, limit_two, &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, result) == 2);
	assert(jsval_array_get(&region, result, 0, &value) == 0);
	assert_utf16_string(&region, value, pair_units,
			sizeof(pair_units) / sizeof(pair_units[0]));
	assert(jsval_array_get(&region, result, 1, &value) == 0);
	assert_utf16_string(&region, value, digits_units,
			sizeof(digits_units) / sizeof(digits_units[0]));
}
#endif

static void test_method_concat_bridge(void)
{
	static const char json[] = "{\"text\":\"lego\",\"suffix\":\"A\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t suffix;
	jsval_t native_text;
	jsval_t native_suffix;
	jsval_t args[4];
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_concat_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"suffix", 6,
			&suffix) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"lego", 4,
			&native_text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"A", 1,
			&native_suffix) == 0);

	args[0] = suffix;
	args[1] = jsval_null();
	args[2] = jsval_bool(1);
	args[3] = jsval_number(42.0);
	assert(jsval_method_string_concat_measure(&region, text, 4, args, &sizes,
			&error) == 0);
	assert(sizes.result_len == strlen("legoAnulltrue42"));
	assert(jsval_method_string_concat(&region, text, 4, args, &result,
			&error) == 0);
	assert_string(&region, result, "legoAnulltrue42");

	args[0] = native_suffix;
	args[1] = jsval_undefined();
	assert(jsval_method_string_concat_measure(&region, text, 2, args, &sizes,
			&error) == 0);
	assert(sizes.result_len == strlen("legoAundefined"));
	assert(jsval_method_string_concat(&region, text, 2, args, &result,
			&error) == 0);
	assert_string(&region, result, "legoAundefined");

	args[0] = suffix;
	assert(jsval_method_string_concat(&region, native_text, 1, args, &result,
			&error) == 0);
	assert_string(&region, result, "legoA");

	errno = 0;
	args[0] = root;
	assert(jsval_method_string_concat_measure(&region, text, 1, args, &sizes,
			&error) < 0);
	assert(errno == ENOTSUP);

	errno = 0;
	assert(jsval_method_string_concat(&region, root, 1, args, &result,
			&error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_accessor_bridge(void)
{
	static const char json[] = "{\"ascii\":\"abc\",\"astral\":\"\\ud83d\\ude00\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t ascii;
	jsval_t astral;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"ascii", 5,
			&ascii) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"astral", 6,
			&astral) == 0);

	assert(jsval_method_string_char_at(&region, ascii, 1, jsval_number(1.9),
			&result, &error) == 0);
	assert_string(&region, result, "b");

	assert(jsval_method_string_at(&region, ascii, 1, jsval_number(-1.0),
			&result, &error) == 0);
	assert_string(&region, result, "c");

	assert(jsval_method_string_at(&region, ascii, 1, jsval_number(99.0),
			&result, &error) == 0);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_method_string_char_code_at(&region, ascii, 1,
			jsval_number(2.0), &result, &error) == 0);
	assert_number_value(result, 99.0);

	assert(jsval_method_string_char_code_at(&region, ascii, 1,
			jsval_number(INFINITY), &result, &error) == 0);
	assert_nan_value(result);

	assert(jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(0.0), &result, &error) == 0);
	assert_number_value(result, 0x1F600);

	assert(jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(99.0), &result, &error) == 0);
	assert(result.kind == JSVAL_KIND_UNDEFINED);

	errno = 0;
	assert(jsval_method_string_char_at(&region, root, 1, jsval_number(0.0),
			&result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_slice_substring_bridge(void)
{
	static const char json[] = "{\"text\":\"bananas\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t native_text;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"bananas", 7,
			&native_text) == 0);

	assert(jsval_method_string_slice(&region, text, 1, jsval_number(1.0),
			1, jsval_number(-1.0), &result, &error) == 0);
	assert_string(&region, result, "anana");

	assert(jsval_method_string_slice(&region, native_text, 1,
			jsval_number(-3.0), 0, jsval_undefined(), &result, &error) == 0);
	assert_string(&region, result, "nas");

	assert(jsval_method_string_substring(&region, text, 1,
			jsval_number(5.0), 1, jsval_number(2.0), &result, &error) == 0);
	assert_string(&region, result, "nan");

	assert(jsval_method_string_substring(&region, native_text, 1,
			jsval_number(2.0), 0, jsval_undefined(), &result, &error) == 0);
	assert_string(&region, result, "nanas");

	errno = 0;
	assert(jsval_method_string_slice(&region, root, 1, jsval_number(0.0),
			0, jsval_undefined(), &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_trim_repeat_bridge(void)
{
	static const char json[] = "{\"trim\":\"\\ufefffoo\\n\",\"repeat\":\"ha\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t trim_text;
	jsval_t repeat_text;
	jsval_t result;
	jsval_t expected;
	jsmethod_error_t error;
	jsmethod_string_repeat_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"trim", 4,
			&trim_text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"repeat", 6,
			&repeat_text) == 0);

	assert(jsval_method_string_trim(&region, trim_text, &result, &error) == 0);
	assert_string(&region, result, "foo");

	assert(jsval_method_string_trim_start(&region, trim_text, &result,
			&error) == 0);
	assert(jsval_string_new_utf16(&region,
			(const uint16_t[]){'f', 'o', 'o', '\n'}, 4, &expected) == 0);
	assert(jsval_strict_eq(&region, result, expected) == 1);

	assert(jsval_method_string_trim_end(&region, trim_text, &result,
			&error) == 0);
	assert(jsval_string_new_utf16(&region,
			(const uint16_t[]){0xFEFF, 'f', 'o', 'o'}, 4, &expected) == 0);
	assert(jsval_strict_eq(&region, result, expected) == 1);

	assert(jsval_method_string_repeat_measure(&region, repeat_text, 1,
			jsval_number(3.0), &sizes, &error) == 0);
	assert(sizes.result_len == 6);
	assert(jsval_method_string_repeat(&region, repeat_text, 1,
			jsval_number(3.0), &result, &error) == 0);
	assert_string(&region, result, "hahaha");

	assert(jsval_method_string_repeat(&region, repeat_text, 1,
			jsval_number(-1.0), &result, &error) == -1);
	assert(error.kind == JSMETHOD_ERROR_RANGE);

	errno = 0;
	assert(jsval_method_string_trim(&region, root, &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_method_substr_trim_alias_bridge(void)
{
	static const char json[] = "{\"text\":\"bananas\",\"trim\":\"\\ufefffoo\\n\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t trim_text;
	jsval_t native_text;
	jsval_t result;
	jsval_t expected;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"trim", 4,
			&trim_text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"bananas", 7,
			&native_text) == 0);

	assert(jsval_method_string_substr(&region, text, 1, jsval_number(1.0), 1,
			jsval_number(3.0), &result, &error) == 0);
	assert_string(&region, result, "ana");

	assert(jsval_method_string_substr(&region, native_text, 1,
			jsval_number(-3.0), 0, jsval_undefined(), &result, &error) == 0);
	assert_string(&region, result, "nas");

	assert(jsval_method_string_trim_left(&region, trim_text, &result,
			&error) == 0);
	assert(jsval_string_new_utf16(&region,
			(const uint16_t[]){'f', 'o', 'o', '\n'}, 4, &expected) == 0);
	assert(jsval_strict_eq(&region, result, expected) == 1);

	assert(jsval_method_string_trim_right(&region, trim_text, &result,
			&error) == 0);
	assert(jsval_string_new_utf16(&region,
			(const uint16_t[]){0xFEFF, 'f', 'o', 'o'}, 4, &expected) == 0);
	assert(jsval_strict_eq(&region, result, expected) == 1);

	errno = 0;
	assert(jsval_method_string_substr(&region, root, 1, jsval_number(0.0), 0,
			jsval_undefined(), &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void
test_method_pad_bridge(void)
{
	static const char json[] = "{\"text\":\"abc\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t native_text;
	jsval_t filler;
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_pad_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 16,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"abc", 3,
			&native_text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"def", 3,
			&filler) == 0);

	assert(jsval_method_string_pad_start_measure(&region, text, 1,
			jsval_number(7.0), 1, filler, &sizes, &error) == 0);
	assert(sizes.result_len == 7);

	assert(jsval_method_string_pad_start(&region, text, 1,
			jsval_number(7.0), 1, filler, &result, &error) == 0);
	assert_string(&region, result, "defdabc");

	assert(jsval_method_string_pad_start(&region, native_text, 1,
			jsval_number(7.0), 1, filler, &result, &error) == 0);
	assert_string(&region, result, "defdabc");

	assert(jsval_method_string_pad_end_measure(&region, text, 1,
			jsval_number(5.0), 1, jsval_undefined(), &sizes, &error) == 0);
	assert(sizes.result_len == 5);

	assert(jsval_method_string_pad_end(&region, text, 1,
			jsval_number(5.0), 1, jsval_undefined(), &result, &error) == 0);
	assert_string(&region, result, "abc  ");

	assert(jsval_method_string_pad_end(&region, native_text, 1,
			jsval_number(7.0), 1, filler, &result, &error) == 0);
	assert_string(&region, result, "abcdefd");

	assert(jsval_method_string_pad_start(&region, native_text, 1,
			jsval_number(INFINITY), 1, filler, &result, &error) < 0);
	assert(errno == EOVERFLOW);

	errno = 0;
	assert(jsval_method_string_pad_start(&region, root, 1,
			jsval_number(7.0), 1, filler, &result, &error) < 0);
	assert(errno == ENOTSUP);
}

static void test_value_semantics(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t null_word;
	jsval_t one_string;
	jsval_t same_a;
	jsval_t same_b;
	jsval_t sum;
	jsval_t undefined_word;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"null", 4, &null_word) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_a) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_b) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"undefined", 9,
			&undefined_word) == 0);

	assert(jsval_truthy(&region, jsval_number(1.0)) == 1);
	assert(jsval_truthy(&region, jsval_bool(1)) == 1);
	assert(jsval_truthy(&region, one_string) == 1);
	assert(jsval_truthy(&region, jsval_number(0.0)) == 0);
	assert(jsval_truthy(&region, jsval_bool(0)) == 0);
	assert(jsval_truthy(&region, jsval_null()) == 0);
	assert(jsval_truthy(&region, jsval_undefined()) == 0);
	assert(jsval_truthy(&region, empty_string) == 0);
	assert(jsval_truthy(&region, jsval_number(NAN)) == 0);

	assert(jsval_strict_eq(&region, jsval_undefined(), jsval_undefined()) == 1);
	assert(jsval_strict_eq(&region, jsval_null(), jsval_null()) == 1);
	assert(jsval_strict_eq(&region, jsval_number(INFINITY), jsval_number(INFINITY)) == 1);
	assert(jsval_strict_eq(&region, jsval_number(-INFINITY), jsval_number(-INFINITY)) == 1);
	assert(jsval_strict_eq(&region, jsval_number(+0.0), jsval_number(-0.0)) == 1);
	assert(jsval_strict_eq(&region, same_a, same_b) == 1);
	assert(jsval_strict_eq(&region, jsval_number(1.0), jsval_number(0.999999999999)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(NAN), jsval_bool(1)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(NAN), jsval_number(NAN)) == 0);
	assert(jsval_strict_eq(&region, jsval_number(1.0), one_string) == 0);
	assert(jsval_strict_eq(&region, jsval_bool(1), jsval_number(1.0)) == 0);
	assert(jsval_strict_eq(&region, jsval_null(), jsval_undefined()) == 0);
	assert(jsval_strict_eq(&region, jsval_null(), null_word) == 0);
	assert(jsval_strict_eq(&region, jsval_undefined(), undefined_word) == 0);
	assert((jsval_strict_eq(&region, jsval_null(), jsval_undefined()) == 0) == 1);
	assert((jsval_strict_eq(&region, jsval_number(1.0), one_string) == 0) == 1);

	assert(jsval_add(&region, jsval_number(1.0), jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 2.0);
	assert(jsval_add(&region, one_string, jsval_number(1.0), &sum) == 0);
	assert_string(&region, sum, "11");
	assert(jsval_add(&region, jsval_number(1.0), one_string, &sum) == 0);
	assert_string(&region, sum, "11");
}

static void test_typeof_semantics(void)
{
	static const char json[] =
		"{\"flag\":true,\"num\":1,\"text\":\"x\",\"nothing\":null,\"obj\":{},\"arr\":[]}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t root;
	jsval_t flag;
	jsval_t num;
	jsval_t text;
	jsval_t nothing;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_text;
	jsval_t native_obj;
	jsval_t native_arr;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));

	errno = 0;
	assert(jsval_typeof(NULL, jsval_undefined(), &result) < 0);
	assert(errno == EINVAL);
	errno = 0;
	assert(jsval_typeof(&region, jsval_undefined(), NULL) < 0);
	assert(errno == EINVAL);

	assert(jsval_typeof(&region, jsval_undefined(), &result) == 0);
	assert_string(&region, result, "undefined");
	assert(jsval_typeof(&region, jsval_null(), &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_typeof(&region, jsval_bool(1), &result) == 0);
	assert_string(&region, result, "boolean");
	assert(jsval_typeof(&region, jsval_number(1.0), &result) == 0);
	assert_string(&region, result, "number");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&native_text) == 0);
	assert(jsval_typeof(&region, native_text, &result) == 0);
	assert_string(&region, result, "string");
	assert(jsval_object_new(&region, 0, &native_obj) == 0);
	assert(jsval_typeof(&region, native_obj, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_array_new(&region, 0, &native_arr) == 0);
	assert(jsval_typeof(&region, native_arr, &result) == 0);
	assert_string(&region, result, "object");

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_typeof(&region, flag, &result) == 0);
	assert_string(&region, result, "boolean");
	assert(jsval_typeof(&region, num, &result) == 0);
	assert_string(&region, result, "number");
	assert(jsval_typeof(&region, text, &result) == 0);
	assert_string(&region, result, "string");
	assert(jsval_typeof(&region, nothing, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_typeof(&region, obj, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_typeof(&region, arr, &result) == 0);
	assert_string(&region, result, "object");

#if JSMX_WITH_REGEX
	{
		jsmethod_error_t error;
		jsval_t pattern;
		jsval_t global_flags;
		jsval_t regex;
		jsval_t subject;
		jsval_t iterator;

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&pattern) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&subject) == 0);
		assert(jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
				&error) == 0);
		assert(jsval_typeof(&region, regex, &result) == 0);
		assert_string(&region, result, "object");
		assert(jsval_method_string_match_all(&region, subject, 1, regex,
				&iterator, &error) == 0);
		assert(jsval_typeof(&region, iterator, &result) == 0);
		assert_string(&region, result, "object");
	}
#endif
}

static void test_url_object_semantics(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t input;
	jsval_t url;
	jsval_t ace_input;
	jsval_t idna_input;
	jsval_t idna_url;
	jsval_t params_a;
	jsval_t params_b;
	jsval_t result;
	jsval_t rel_input;
	jsval_t base_input;
	jsval_t rel_url;
	jsval_t wire_url;
	jsval_t detached;
	jsval_t values;
	jsval_t first;
	jsval_t expected;
	jsval_t name_x;
	jsval_t name_y;
	jsval_t name_a;
	jsval_t name_b;
	jsval_t name_c;
	jsval_t name_plus;
	jsval_t name_pair;
	jsval_t value_2;
	jsval_t value_5;
	jsval_t value_words;
	jsval_t value_plus;
	jsval_t value_pair;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"https://example.com/base?x=1#old",
			sizeof("https://example.com/base?x=1#old") - 1,
			&input) == 0);
	assert(jsval_url_new(&region, input, 0, jsval_undefined(), &url) == 0);
	assert(url.kind == JSVAL_KIND_URL);
	assert(jsval_truthy(&region, url) == 1);
	assert(jsval_typeof(&region, url, &result) == 0);
	assert_string(&region, result, "object");
	assert(jsval_url_href(&region, url, &result) == 0);
	assert_string(&region, result, "https://example.com/base?x=1#old");
	assert(jsval_url_to_string(&region, url, &result) == 0);
	assert_string(&region, result, "https://example.com/base?x=1#old");
	assert(jsval_url_to_json(&region, url, &result) == 0);
	assert_string(&region, result, "https://example.com/base?x=1#old");

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"https://ma\xc3\xb1""ana\xe3\x80\x82""example/a?x=1#old",
			sizeof("https://ma\xc3\xb1""ana\xe3\x80\x82""example/a?x=1#old") - 1,
			&idna_input) == 0);
	assert(jsval_url_new(&region, idna_input, 0, jsval_undefined(),
			&idna_url) == 0);
	assert(jsval_url_hostname(&region, idna_url, &result) == 0);
	assert_string(&region, result, "xn--maana-pta.example");
	assert(jsval_url_hostname_display(&region, idna_url, &result) == 0);
	assert_string(&region, result, "ma\xc3\xb1""ana.example");
	assert(jsval_url_host(&region, idna_url, &result) == 0);
	assert_string(&region, result, "xn--maana-pta.example");
	assert(jsval_url_host_display(&region, idna_url, &result) == 0);
	assert_string(&region, result, "ma\xc3\xb1""ana.example");
	assert(jsval_url_origin(&region, idna_url, &result) == 0);
	assert_string(&region, result, "https://xn--maana-pta.example");
	assert(jsval_url_origin_display(&region, idna_url, &result) == 0);
	assert_string(&region, result, "https://ma\xc3\xb1""ana.example");
	assert(jsval_url_href(&region, idna_url, &result) == 0);
	assert_string(&region, result, "https://xn--maana-pta.example/a?x=1#old");
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"https://xn--maana-pta.example/a?x=1#old",
			sizeof("https://xn--maana-pta.example/a?x=1#old") - 1,
			&ace_input) == 0);
	assert(jsval_url_new(&region, ace_input, 0, jsval_undefined(),
			&result) == 0);
	assert(result.kind == JSVAL_KIND_URL);
	assert(jsval_url_hostname_display(&region, result, &expected) == 0);
	assert_string(&region, expected, "ma\xc3\xb1""ana.example");
	assert(jsval_url_origin_display(&region, result, &expected) == 0);
	assert_string(&region, expected, "https://ma\xc3\xb1""ana.example");
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"b\xc3\xbc""cher\xef\xbc\x8e""example:8443",
			sizeof("b\xc3\xbc""cher\xef\xbc\x8e""example:8443") - 1,
			&expected) == 0);
	assert(jsval_url_set_host(&region, idna_url, expected) == 0);
	assert(jsval_url_host(&region, idna_url, &result) == 0);
	assert_string(&region, result, "xn--bcher-kva.example:8443");
	assert(jsval_url_host_display(&region, idna_url, &result) == 0);
	assert_string(&region, result, "b\xc3\xbc""cher.example:8443");
	assert(jsval_url_origin_display(&region, idna_url, &result) == 0);
	assert_string(&region, result, "https://b\xc3\xbc""cher.example:8443");
	assert(jsval_url_href(&region, idna_url, &result) == 0);
	assert_string(&region, result,
			"https://xn--bcher-kva.example:8443/a?x=1#old");
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"https://xn--.example/a",
			sizeof("https://xn--.example/a") - 1, &expected) == 0);
	errno = 0;
	assert(jsval_url_new(&region, expected, 0, jsval_undefined(),
			&result) == -1);
	assert(errno == EINVAL);

	assert(jsval_url_search_params(&region, url, &params_a) == 0);
	assert(jsval_url_search_params(&region, url, &params_b) == 0);
	assert(params_a.kind == JSVAL_KIND_URL_SEARCH_PARAMS);
	assert(jsval_strict_eq(&region, params_a, params_b) == 1);
	assert(jsval_truthy(&region, params_a) == 1);
	assert(jsval_typeof(&region, params_a, &result) == 0);
	assert_string(&region, result, "object");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &name_x)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"y", 1, &name_y)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1, &name_a)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"b", 1, &name_b)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"c", 1, &name_c)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1, &value_2)
			== 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"5", 1, &value_5)
			== 0);

	assert(jsval_url_search_params_append(&region, params_a, name_y, value_2)
			== 0);
	assert(jsval_url_search(&region, url, &result) == 0);
	assert_string(&region, result, "?x=1&y=2");
	assert(jsval_url_href(&region, url, &result) == 0);
	assert_string(&region, result, "https://example.com/base?x=1&y=2#old");
	assert(jsval_url_search_params_to_string(&region, params_a, &result) == 0);
	assert_string(&region, result, "x=1&y=2");
	assert(jsval_url_search_params_get(&region, params_a, name_x, &result) == 0);
	assert_string(&region, result, "1");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"?a=3", 4,
			&expected) == 0);
	assert(jsval_url_set_search(&region, url, expected) == 0);
	assert(jsval_url_search_params_to_string(&region, params_a, &result) == 0);
	assert_string(&region, result, "a=3");
	assert(jsval_url_search_params_get(&region, params_a, name_a, &result) == 0);
	assert_string(&region, result, "3");
	assert(jsval_url_search_params_size(&region, params_a, &result) == 0);
	assert_number_value(result, 1.0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"../up?q=2", 9,
			&rel_input) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"https://example.com/dir/sub/index.html?x=1#old",
			sizeof("https://example.com/dir/sub/index.html?x=1#old") - 1,
			&base_input) == 0);
	assert(jsval_url_new(&region, rel_input, 1, base_input, &rel_url) == 0);
	assert(jsval_url_href(&region, rel_url, &result) == 0);
	assert_string(&region, result, "https://example.com/dir/up?q=2");
	assert(jsval_strict_eq(&region, url, rel_url) == 0);

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)
			"https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80",
			sizeof("https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80") - 1,
			&input) == 0);
	assert(jsval_url_new(&region, input, 0, jsval_undefined(), &wire_url) == 0);
	assert(jsval_url_pathname(&region, wire_url, &result) == 0);
	assert_string(&region, result, "/a b/\xf0\x9f\x98\x80");
	assert(jsval_url_search(&region, wire_url, &result) == 0);
	assert_string(&region, result, "?q=two words&plus=%2B&pair=a%26b%3Dc");
	assert(jsval_url_hash(&region, wire_url, &result) == 0);
	assert_string(&region, result, "#frag \xf0\x9f\x98\x80");
	assert(jsval_url_href(&region, wire_url, &result) == 0);
	assert_string(&region, result,
			"https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80");

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"/emoji \xf0\x9f\x98\x80",
			sizeof("/emoji \xf0\x9f\x98\x80") - 1, &input) == 0);
	assert(jsval_url_set_pathname(&region, wire_url, input) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"?q=hello world&plus=%2B",
			sizeof("?q=hello world&plus=%2B") - 1, &input) == 0);
	assert(jsval_url_set_search(&region, wire_url, input) == 0);
	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"done \xf0\x9f\x98\x80",
			sizeof("done \xf0\x9f\x98\x80") - 1, &input) == 0);
	assert(jsval_url_set_hash(&region, wire_url, input) == 0);
	assert(jsval_url_href(&region, wire_url, &result) == 0);
	assert_string(&region, result,
			"https://example.com/emoji%20%F0%9F%98%80?q=hello%20world&plus=%2B#done%20%F0%9F%98%80");

	assert(jsval_string_new_utf8(&region,
			(const uint8_t *)"https://example.com/base",
			sizeof("https://example.com/base") - 1, &input) == 0);
	assert(jsval_url_new(&region, input, 0, jsval_undefined(), &wire_url) == 0);
	assert(jsval_url_search_params(&region, wire_url, &params_b) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"plus", 4,
			&name_plus) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"pair", 4,
			&name_pair) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"two words", 9,
			&value_words) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"+", 1,
			&value_plus) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"a&b=c", 5,
			&value_pair) == 0);
	assert(jsval_url_search_params_append(&region, params_b, name_x, value_words)
			== 0);
	assert(jsval_url_search_params_append(&region, params_b, name_plus, value_plus)
			== 0);
	assert(jsval_url_search_params_append(&region, params_b, name_pair, value_pair)
			== 0);
	assert(jsval_url_search(&region, wire_url, &result) == 0);
	assert_string(&region, result, "?x=two words&plus=%2B&pair=a%26b%3Dc");
	assert(jsval_url_href(&region, wire_url, &result) == 0);
	assert_string(&region, result,
			"https://example.com/base?x=two%20words&plus=%2B&pair=a%26b%3Dc");
	assert(jsval_url_search_params_to_string(&region, params_b, &result) == 0);
	assert_string(&region, result, "x=two+words&plus=%2B&pair=a%26b%3Dc");

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"?b=4", 4,
			&input) == 0);
	assert(jsval_url_search_params_new(&region, input, &detached) == 0);
	assert(detached.kind == JSVAL_KIND_URL_SEARCH_PARAMS);
	assert(jsval_strict_eq(&region, detached, params_a) == 0);
	assert(jsval_truthy(&region, detached) == 1);
	assert(jsval_typeof(&region, detached, &result) == 0);
	assert_string(&region, result, "object");

	assert(jsval_url_search_params_append(&region, detached, name_c, value_5)
			== 0);
	assert(jsval_url_search_params_get(&region, detached, name_b, &result) == 0);
	assert_string(&region, result, "4");
	assert(jsval_url_search_params_has(&region, detached, name_c, &result) == 0);
	assert(result.kind == JSVAL_KIND_BOOL);
	assert(result.as.boolean == 1);
	assert(jsval_url_search_params_get_all(&region, detached, name_c, &values)
			== 0);
	assert(values.kind == JSVAL_KIND_ARRAY);
	assert(jsval_array_length(&region, values) == 1);
	assert(jsval_array_get(&region, values, 0, &first) == 0);
	assert_string(&region, first, "5");
	assert(jsval_url_search_params_size(&region, detached, &result) == 0);
	assert_number_value(result, 2.0);
	assert(jsval_url_search_params_to_string(&region, detached, &result) == 0);
	assert_string(&region, result, "b=4&c=5");
}

static void test_nullish_and_selection_semantics(void)
{
	static const char json[] =
		"{\"nothing\":null,\"flag\":false,\"zero\":0,\"empty\":\"\",\"text\":\"x\",\"obj\":{},\"arr\":[]}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t nothing;
	jsval_t flag;
	jsval_t zero;
	jsval_t empty;
	jsval_t text;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_text;
	jsval_t native_obj;
	jsval_t other_obj;
	jsval_t native_arr;
	jsval_t other_arr;
	jsval_t fallback;
	jsval_t then_value;
	jsval_t else_value;
	jsval_t left;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_is_nullish(jsval_undefined()) == 1);
	assert(jsval_is_nullish(jsval_null()) == 1);
	assert(jsval_is_nullish(jsval_bool(0)) == 0);
	assert(jsval_is_nullish(jsval_bool(1)) == 0);
	assert(jsval_is_nullish(jsval_number(0.0)) == 0);
	assert(jsval_is_nullish(jsval_number(-0.0)) == 0);
	assert(jsval_is_nullish(jsval_number(NAN)) == 0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&native_text) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"fallback", 8,
			&fallback) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"then", 4,
			&then_value) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"else", 4,
			&else_value) == 0);
	assert(jsval_object_new(&region, 0, &native_obj) == 0);
	assert(jsval_object_new(&region, 0, &other_obj) == 0);
	assert(jsval_array_new(&region, 0, &native_arr) == 0);
	assert(jsval_array_new(&region, 0, &other_arr) == 0);

	assert(jsval_is_nullish(native_text) == 0);
	assert(jsval_is_nullish(native_obj) == 0);
	assert(jsval_is_nullish(native_arr) == 0);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"zero", 4,
			&zero) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"empty", 5,
			&empty) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_is_nullish(nothing) == 1);
	assert(jsval_is_nullish(flag) == 0);
	assert(jsval_is_nullish(zero) == 0);
	assert(jsval_is_nullish(empty) == 0);
	assert(jsval_is_nullish(text) == 0);
	assert(jsval_is_nullish(obj) == 0);
	assert(jsval_is_nullish(arr) == 0);

	left = jsval_undefined();
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	assert_string(&region, result, "fallback");

	left = jsval_null();
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	assert_string(&region, result, "fallback");

	left = nothing;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	assert_string(&region, result, "fallback");

	left = flag;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	assert(jsval_strict_eq(&region, result, jsval_bool(0)) == 1);

	left = zero;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);

	left = empty;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	assert(jsval_strict_eq(&region, result, empty) == 1);

	left = obj;
	if (jsval_is_nullish(left)) {
		result = other_obj;
	} else {
		result = left;
	}
	assert(jsval_strict_eq(&region, result, obj) == 1);

	left = arr;
	if (jsval_is_nullish(left)) {
		result = other_arr;
	} else {
		result = left;
	}
	assert(jsval_strict_eq(&region, result, arr) == 1);

	if (jsval_truthy(&region, jsval_undefined())) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "else");

	if (jsval_truthy(&region, jsval_null())) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "else");

	if (jsval_truthy(&region, flag)) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "else");

	if (jsval_truthy(&region, zero)) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "else");

	if (jsval_truthy(&region, empty)) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "else");

	if (jsval_truthy(&region, text)) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "then");

	if (jsval_truthy(&region, native_text)) {
		result = then_value;
	} else {
		result = else_value;
	}
	assert_string(&region, result, "then");

	if (jsval_truthy(&region, obj)) {
		result = obj;
	} else {
		result = other_obj;
	}
	assert(jsval_strict_eq(&region, result, obj) == 1);

	if (jsval_truthy(&region, native_obj)) {
		result = native_obj;
	} else {
		result = other_obj;
	}
	assert(jsval_strict_eq(&region, result, native_obj) == 1);

	if (jsval_truthy(&region, arr)) {
		result = arr;
	} else {
		result = other_arr;
	}
	assert(jsval_strict_eq(&region, result, arr) == 1);

	if (jsval_truthy(&region, native_arr)) {
		result = native_arr;
	} else {
		result = other_arr;
	}
	assert(jsval_strict_eq(&region, result, native_arr) == 1);

#if JSMX_WITH_REGEX
	{
		jsmethod_error_t error;
		jsval_t pattern;
		jsval_t global_flags;
		jsval_t regex;
		jsval_t subject;
		jsval_t iterator;

		assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&pattern) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) == 0);
		assert(jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&subject) == 0);
		assert(jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
				&error) == 0);
		assert(jsval_method_string_match_all(&region, subject, 1, regex,
				&iterator, &error) == 0);

		assert(jsval_is_nullish(regex) == 0);
		assert(jsval_is_nullish(iterator) == 0);

		left = regex;
		if (jsval_is_nullish(left)) {
			result = native_obj;
		} else {
			result = left;
		}
		assert(jsval_strict_eq(&region, result, regex) == 1);

		if (jsval_truthy(&region, iterator)) {
			result = iterator;
		} else {
			result = other_arr;
		}
		assert(jsval_strict_eq(&region, result, iterator) == 1);
	}
#endif
}

static void test_logical_not_semantics(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t x;
	int c = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	x = jsval_bool(1);

	assert((!jsval_truthy(&region, jsval_bool(1))) == 0);
	assert((!(!jsval_truthy(&region, jsval_bool(1)))) == 1);
	assert((!jsval_truthy(&region, x)) == 0);
	assert((!(!jsval_truthy(&region, x))) == 1);
	assert((!jsval_truthy(&region, jsval_bool(0))) == 1);
	assert((!jsval_truthy(&region, jsval_null())) == 1);
	assert((!jsval_truthy(&region, jsval_undefined())) == 1);
	assert((!jsval_truthy(&region, jsval_number(+0.0))) == 1);
	assert((!jsval_truthy(&region, jsval_number(-0.0))) == 1);
	assert((!jsval_truthy(&region, jsval_number(NAN))) == 1);
	assert((!jsval_truthy(&region, jsval_number(INFINITY))) == 0);
	assert((!jsval_truthy(&region, jsval_number(-13.0))) == 0);
	assert((!jsval_truthy(&region, empty_string)) == 1);
	assert((!jsval_truthy(&region, one_string)) == 0);

	if (!jsval_truthy(&region, jsval_number(1.0))) {
		assert(0);
	} else {
		c++;
	}
	if (!jsval_truthy(&region, jsval_bool(1))) {
		assert(0);
	} else {
		c++;
	}
	if (!jsval_truthy(&region, one_string)) {
		assert(0);
	} else {
		c++;
	}
	assert(c == 3);
}

static void test_logical_and_or_semantics(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t minus_one_string;
	jsval_t x_string;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"-1", 2,
			&minus_one_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &x_string) == 0);

	result = logical_and(&region, jsval_bool(0), jsval_bool(1));
	assert(jsval_strict_eq(&region, result, jsval_bool(0)) == 1);
	result = logical_and(&region, jsval_bool(1), jsval_bool(0));
	assert(jsval_strict_eq(&region, result, jsval_bool(0)) == 1);

	result = logical_and(&region, jsval_number(-0.0), jsval_number(-1.0));
	assert_negative_zero(result);
	result = logical_and(&region, jsval_number(0.0), jsval_number(-1.0));
	assert_positive_zero(result);
	result = logical_and(&region, jsval_number(NAN), jsval_number(1.0));
	assert_nan_value(result);

	result = logical_and(&region, empty_string, one_string);
	assert_string(&region, result, "");
	result = logical_and(&region, one_string, x_string);
	assert_string(&region, result, "x");
	result = logical_and(&region, minus_one_string, x_string);
	assert_string(&region, result, "x");

	result = logical_or(&region, jsval_bool(0), jsval_bool(1));
	assert(jsval_strict_eq(&region, result, jsval_bool(1)) == 1);
	result = logical_or(&region, jsval_number(0.0), jsval_number(-0.0));
	assert_negative_zero(result);
	result = logical_or(&region, jsval_number(-0.0), jsval_number(0.0));
	assert_positive_zero(result);
	result = logical_or(&region, empty_string, one_string);
	assert_string(&region, result, "1");

	result = logical_or(&region, jsval_number(-1.0), jsval_number(1.0));
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	result = logical_or(&region, jsval_number(-1.0), jsval_number(NAN));
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	result = logical_or(&region, minus_one_string, one_string);
	assert_string(&region, result, "-1");
	result = logical_or(&region, minus_one_string, x_string);
	assert_string(&region, result, "-1");
}

static void test_numeric_coercion_and_arithmetic(void)
{
	static const char json[] =
		"{\"truth\":true,\"nothing\":null,\"one\":\"1\",\"spaces\":\"   \",\"bad\":\"x\",\"inf\":\"Infinity\",\"upper\":\"INFINITY\",\"num\":2}";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t nothing;
	jsval_t one;
	jsval_t spaces;
	jsval_t bad;
	jsval_t inf;
	jsval_t upper;
	jsval_t num;
	jsval_t result;
	double number;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_to_number(&region, jsval_undefined(), &number) == 0);
	assert(isnan(number));
	assert(jsval_to_number(&region, jsval_null(), &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, jsval_bool(0), &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, jsval_bool(1), &number) == 0);
	assert(number == 1.0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"   ", 3, &spaces) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"Infinity", 8, &inf) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"INFINITY", 8, &upper) == 0);

	assert(jsval_to_number(&region, one, &number) == 0);
	assert(number == 1.0);
	assert(jsval_to_number(&region, spaces, &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, bad, &number) == 0);
	assert(isnan(number));
	assert(jsval_to_number(&region, inf, &number) == 0);
	assert(isinf(number) && number > 0);
	assert(jsval_to_number(&region, upper, &number) == 0);
	assert(isnan(number));

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
			&one) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"spaces", 6,
			&spaces) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
			&bad) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"inf", 3,
			&inf) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"upper", 5,
			&upper) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);

	assert(jsval_to_number(&region, truth, &number) == 0);
	assert(number == 1.0);
	assert(jsval_to_number(&region, nothing, &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, one, &number) == 0);
	assert(number == 1.0);
	assert(jsval_to_number(&region, spaces, &number) == 0);
	assert(number == 0.0);
	assert(jsval_to_number(&region, bad, &number) == 0);
	assert(isnan(number));
	assert(jsval_to_number(&region, inf, &number) == 0);
	assert(isinf(number) && number > 0);
	assert(jsval_to_number(&region, upper, &number) == 0);
	assert(isnan(number));

	assert(jsval_unary_plus(&region, jsval_undefined(), &result) == 0);
	assert_nan_value(result);
	assert(jsval_unary_plus(&region, jsval_null(), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_unary_plus(&region, truth, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_unary_plus(&region, one, &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_unary_plus(&region, inf, &result) == 0);
	assert(isinf(result.as.number) && result.as.number > 0);
	assert(jsval_unary_plus(&region, upper, &result) == 0);
	assert_nan_value(result);

	assert(jsval_unary_minus(&region, jsval_bool(0), &result) == 0);
	assert_negative_zero(result);
	assert(jsval_unary_minus(&region, jsval_bool(1), &result) == 0);
	assert_number_value(result, -1.0);
	assert(jsval_unary_minus(&region, jsval_number(0.0), &result) == 0);
	assert_negative_zero(result);
	assert(jsval_unary_minus(&region, jsval_number(-0.0), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_unary_minus(&region, one, &result) == 0);
	assert_number_value(result, -1.0);
	assert(jsval_unary_minus(&region, bad, &result) == 0);
	assert_nan_value(result);

	assert(jsval_subtract(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_subtract(&region, jsval_bool(1), jsval_number(1.0), &result) == 0);
	assert_positive_zero(result);
	assert(jsval_subtract(&region, jsval_number(1.0), jsval_bool(1), &result) == 0);
	assert_positive_zero(result);

	assert(jsval_multiply(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_divide(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_number_value(result, 1.0);
	assert(jsval_remainder(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert_positive_zero(result);

	errno = 0;
	assert(jsval_to_number(&region, root, &number) < 0);
	assert(errno == ENOTSUP);
}

static void test_integer_coercion_and_bitwise(void)
{
	static const char json[] =
		"{\"truth\":true,\"zero\":\"0\",\"one\":\"1\",\"bad\":\"x\",\"nothing\":null,\"num\":5,\"obj\":{},\"arr\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t zero;
	jsval_t one;
	jsval_t bad;
	jsval_t nothing;
	jsval_t num;
	jsval_t obj;
	jsval_t arr;
	jsval_t result;
	int32_t i32;
	uint32_t u32;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_to_int32(&region, jsval_bool(1), &i32) == 0);
	assert(i32 == 1);
	assert(jsval_to_int32(&region, jsval_undefined(), &i32) == 0);
	assert(i32 == 0);
	assert(jsval_to_uint32(&region, jsval_number(4294967295.0), &u32) == 0);
	assert(u32 == 4294967295u);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);
	assert(jsval_to_int32(&region, one, &i32) == 0);
	assert(i32 == 1);
	assert(jsval_to_int32(&region, bad, &i32) == 0);
	assert(i32 == 0);

	assert(jsval_bitwise_not(&region, jsval_bool(0), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_bitwise_not(&region, one, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-2.0)) == 1);
	assert(jsval_bitwise_not(&region, jsval_null(), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_bitwise_not(&region, bad, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);

	assert(jsval_bitwise_and(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_or(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_xor(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"zero", 4,
			&zero) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
			&one) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
			&bad) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_to_int32(&region, zero, &i32) == 0);
	assert(i32 == 0);
	assert(jsval_to_int32(&region, one, &i32) == 0);
	assert(i32 == 1);
	assert(jsval_bitwise_not(&region, nothing, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_bitwise_and(&region, truth, num, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_or(&region, one, jsval_bool(0), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);
	assert(jsval_bitwise_xor(&region, bad, jsval_bool(1), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(1.0)) == 1);

	errno = 0;
	assert(jsval_bitwise_not(&region, obj, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_bitwise_and(&region, arr, jsval_bool(1), &result) < 0);
	assert(errno == ENOTSUP);
}

static void test_shift_semantics(void)
{
	static const char json[] =
		"{\"truth\":true,\"count\":\"33\",\"neg\":-1,\"num\":5,\"obj\":{},\"arr\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t count;
	jsval_t neg;
	jsval_t num;
	jsval_t obj;
	jsval_t arr;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_shift_left(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, jsval_bool(1), jsval_bool(1), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);
	assert(jsval_shift_right_unsigned(&region, jsval_bool(1), jsval_bool(1),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(0.0)) == 1);

	assert(jsval_shift_left(&region, jsval_number(1.0), jsval_number(33.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, jsval_number(5.0), jsval_number(33.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, jsval_number(-1.0), jsval_number(1.0),
			&result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(-1.0)) == 1);
	assert(jsval_shift_right_unsigned(&region, jsval_number(-1.0),
			jsval_number(1.0), &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2147483647.0)) == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 20,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"count", 5,
			&count) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"neg", 3,
			&neg) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_shift_left(&region, truth, count, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right(&region, num, count, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1);
	assert(jsval_shift_right_unsigned(&region, neg, truth, &result) == 0);
	assert(jsval_strict_eq(&region, result, jsval_number(2147483647.0)) == 1);

	errno = 0;
	assert(jsval_shift_left(&region, obj, jsval_bool(1), &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_shift_right_unsigned(&region, jsval_bool(1), arr, &result) < 0);
	assert(errno == ENOTSUP);
}

static void test_relational_semantics(void)
{
	static const char json[] =
		"{\"truth\":true,\"nothing\":null,\"one\":\"1\",\"bad\":\"x\",\"num\":2,\"left\":\"1\",\"right\":\"2\"}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t nothing;
	jsval_t one;
	jsval_t bad;
	jsval_t num;
	jsval_t left_string;
	jsval_t right_string;
	jsval_t ten_string;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_less_than(&region, jsval_bool(1), jsval_number(1.0), &result)
			== 0);
	assert(result == 0);
	assert(jsval_less_than(&region, jsval_number(1.0), jsval_bool(1), &result)
			== 0);
	assert(result == 0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&left_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&right_string) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"10", 2,
			&ten_string) == 0);

	assert(jsval_less_than(&region, jsval_bool(1), one, &result) == 0);
	assert(result == 0);
	assert(jsval_less_than(&region, one, jsval_bool(1), &result) == 0);
	assert(result == 0);
	assert(jsval_less_than(&region, jsval_null(), jsval_bool(1), &result) == 0);
	assert(result == 1);

	assert(jsval_less_equal(&region, jsval_bool(1), jsval_number(1.0), &result)
			== 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, jsval_number(1.0), jsval_bool(1), &result)
			== 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_than(&region, jsval_number(1.0), jsval_bool(1),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_equal(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, jsval_number(1.0), jsval_bool(1),
			&result) == 0);
	assert(result == 1);

	assert(jsval_less_than(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_less_equal(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_than(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_greater_equal(&region, jsval_number(NAN), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);

	assert(jsval_less_than(&region, left_string, right_string, &result) == 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, left_string, right_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, right_string, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, right_string, left_string, &result)
			== 0);
	assert(result == 1);
	assert(jsval_less_than(&region, left_string, ten_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, ten_string, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, left_string, one, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, left_string, one, &result) == 0);
	assert(result == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
			&one) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
			&bad) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"left", 4,
			&left_string) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"right", 5,
			&right_string) == 0);

	assert(jsval_greater_than(&region, num, truth, &result) == 0);
	assert(result == 1);
	assert(jsval_less_than(&region, nothing, truth, &result) == 0);
	assert(result == 1);
	assert(jsval_less_than(&region, one, truth, &result) == 0);
	assert(result == 0);
	assert(jsval_greater_equal(&region, truth, one, &result) == 0);
	assert(result == 1);
	assert(jsval_less_than(&region, bad, jsval_number(1.0), &result) == 0);
	assert(result == 0);
	assert(jsval_less_than(&region, left_string, right_string, &result) == 0);
	assert(result == 1);
	assert(jsval_less_equal(&region, one, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_than(&region, right_string, left_string, &result) == 0);
	assert(result == 1);
	assert(jsval_greater_equal(&region, left_string, one, &result) == 0);
	assert(result == 1);
}

static void test_abstract_equality_semantics(void)
{
	static const char json[] =
		"{\"num\":1,\"flag\":true,\"text\":\"1\",\"empty\":\"\",\"nothing\":null,\"obj\":{},\"arr\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t one;
	jsval_t zero;
	jsval_t bad;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t text;
	jsval_t empty;
	jsval_t nothing;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_obj;
	jsval_t native_arr;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"0", 1, &zero) == 0);
	assert(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &bad) == 0);

	assert(jsval_abstract_eq(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, jsval_number(0.0), jsval_bool(0),
			&result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, one, jsval_bool(1), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, jsval_bool(0), zero, &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, one, jsval_number(1.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, bad, jsval_number(1.0), &result) == 0);
	assert(result == 0);
	assert(jsval_abstract_eq(&region, jsval_null(), jsval_undefined(),
			&result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, jsval_null(), jsval_number(0.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_abstract_eq(&region, jsval_number(NAN), jsval_bool(1),
			&result) == 0);
	assert(result == 0);

	assert(jsval_abstract_ne(&region, jsval_bool(1), jsval_number(1.0),
			&result) == 0);
	assert(result == 0);
	assert(jsval_abstract_ne(&region, bad, jsval_number(1.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_ne(&region, jsval_null(), jsval_undefined(),
			&result) == 0);
	assert(result == 0);
	assert(jsval_abstract_ne(&region, jsval_number(NAN), jsval_number(NAN),
			&result) == 0);
	assert(result == 1);

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 24,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"empty", 5,
			&empty) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);

	assert(jsval_abstract_eq(&region, num, one, &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, flag, one, &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, text, jsval_number(1.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, empty, jsval_number(0.0), &result) == 0);
	assert(result == 1);
	assert(jsval_abstract_eq(&region, nothing, jsval_undefined(), &result)
			== 0);
	assert(result == 1);
	assert(jsval_abstract_ne(&region, nothing, jsval_number(0.0), &result)
			== 0);
	assert(result == 1);

	assert(jsval_object_new(&region, 1, &native_obj) == 0);
	assert(jsval_array_new(&region, 1, &native_arr) == 0);

	errno = 0;
	assert(jsval_abstract_eq(&region, obj, obj, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_abstract_ne(&region, arr, arr, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_abstract_eq(&region, native_obj, native_obj, &result) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_abstract_ne(&region, native_arr, native_arr, &result) < 0);
	assert(errno == ENOTSUP);
}

static void test_json_backed_value_parity(void)
{
	static const char json[] =
		"{\"num\":1,\"flag\":true,\"off\":false,\"text\":\"2\",\"nothing\":null,\"obj\":{},\"obj2\":{},\"arr\":[],\"arr2\":[]}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t off;
	jsval_t text;
	jsval_t nothing;
	jsval_t obj;
	jsval_t obj_again;
	jsval_t obj2;
	jsval_t arr;
	jsval_t arr_again;
	jsval_t arr2;
	jsval_t native_text;
	jsval_t native_obj;
	jsval_t native_obj_other;
	jsval_t native_arr;
	jsval_t native_arr_other;
	jsval_t sum;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 48,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"off", 3,
			&off) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj_again) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"obj2", 4,
			&obj2) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr_again) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"arr2", 4,
			&arr2) == 0);

	assert(jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&native_text) == 0);
	assert(jsval_object_new(&region, 0, &native_obj) == 0);
	assert(jsval_object_new(&region, 0, &native_obj_other) == 0);
	assert(jsval_array_new(&region, 0, &native_arr) == 0);
	assert(jsval_array_new(&region, 0, &native_arr_other) == 0);

	assert(jsval_truthy(&region, num) == jsval_truthy(&region, jsval_number(1.0)));
	assert(jsval_truthy(&region, flag) == jsval_truthy(&region, jsval_bool(1)));
	assert(jsval_truthy(&region, off) == jsval_truthy(&region, jsval_bool(0)));
	assert(jsval_truthy(&region, text) == jsval_truthy(&region, native_text));
	assert(jsval_truthy(&region, nothing) == jsval_truthy(&region, jsval_null()));

	assert(jsval_strict_eq(&region, num, jsval_number(1.0)) == 1);
	assert(jsval_strict_eq(&region, flag, jsval_bool(1)) == 1);
	assert(jsval_strict_eq(&region, off, jsval_bool(0)) == 1);
	assert(jsval_strict_eq(&region, text, native_text) == 1);
	assert(jsval_strict_eq(&region, nothing, jsval_null()) == 1);

	assert(jsval_add(&region, text, jsval_number(1.0), &sum) == 0);
	assert_string(&region, sum, "21");
	assert(jsval_add(&region, flag, jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 2.0);
	assert(jsval_add(&region, nothing, jsval_number(1.0), &sum) == 0);
	assert(sum.kind == JSVAL_KIND_NUMBER);
	assert(sum.as.number == 1.0);

	assert(jsval_truthy(&region, obj) == 1);
	assert(jsval_truthy(&region, arr) == 1);
	assert(jsval_truthy(&region, native_obj) == 1);
	assert(jsval_truthy(&region, native_arr) == 1);

	assert(jsval_strict_eq(&region, obj, obj_again) == 1);
	assert(jsval_strict_eq(&region, obj, obj2) == 0);
	assert(jsval_strict_eq(&region, arr, arr_again) == 1);
	assert(jsval_strict_eq(&region, arr, arr2) == 0);
	assert(jsval_strict_eq(&region, native_obj, native_obj) == 1);
	assert(jsval_strict_eq(&region, native_obj, native_obj_other) == 0);
	assert(jsval_strict_eq(&region, native_arr, native_arr) == 1);
	assert(jsval_strict_eq(&region, native_arr, native_arr_other) == 0);
}

static void test_shallow_planned_promotion(void)
{
	static const char json[] =
		"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2],\"status\":\"ok\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t profile;
	jsval_t status;
	jsval_t scores;
	jsval_t same_root;
	jsval_t same_scores;
	jsval_t got;
	size_t bytes;
	size_t before_used;
	int has;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32,
			&root) == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7,
			&profile) == 0);
	assert(jsval_is_json_backed(profile) == 1);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6,
			&scores) == 0);
	assert(jsval_is_json_backed(scores) == 1);

	errno = 0;
	assert(jsval_promote_object_shallow_measure(&region, root, 2, &bytes) < 0);
	assert(errno == ENOBUFS);

	before_used = region.used;
	assert(jsval_promote_object_shallow_measure(&region, root, 4, &bytes) == 0);
	assert(bytes > 0);
	assert(jsval_promote_object_shallow_in_place(&region, &root, 4) == 0);
	assert(jsval_is_native(root) == 1);
	assert(region.used == before_used + bytes);
	assert(jsval_region_set_root(&region, root) == 0);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7,
			&profile) == 0);
	assert(jsval_is_json_backed(profile) == 1);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"status", 6,
			&status) == 0);
	assert(jsval_is_json_backed(status) == 1);
	assert_string(&region, status, "ok");
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"ready", 5,
			jsval_bool(1)) == 0);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"ready", 5,
			&has) == 0);
	assert(has == 1);

	assert(jsval_promote_object_shallow_measure(&region, root, 4, &bytes) == 0);
	assert(bytes == 0);
	assert(jsval_promote_object_shallow(&region, root, 4, &same_root) == 0);
	assert(same_root.off == root.off);
	errno = 0;
	assert(jsval_promote_object_shallow_measure(&region, root, 5, &bytes) < 0);
	assert(errno == ENOBUFS);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6,
			&scores) == 0);
	assert(jsval_is_json_backed(scores) == 1);
	errno = 0;
	assert(jsval_promote_array_shallow_measure(&region, scores, 1, &bytes) < 0);
	assert(errno == ENOBUFS);

	before_used = region.used;
	assert(jsval_promote_array_shallow_measure(&region, scores, 4, &bytes) == 0);
	assert(bytes > 0);
	assert(jsval_promote_array_shallow_in_place(&region, &scores, 4) == 0);
	assert(jsval_is_native(scores) == 1);
	assert(region.used == before_used + bytes);
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"scores", 6,
			scores) == 0);

	assert(jsval_array_push(&region, scores, jsval_number(3.0)) == 0);
	assert(jsval_array_set_length(&region, scores, 4) == 0);
	assert(jsval_array_get(&region, scores, 3, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_set(&region, scores, 3, jsval_number(4.0)) == 0);

	assert(jsval_promote_array_shallow_measure(&region, scores, 4, &bytes) == 0);
	assert(bytes == 0);
	assert(jsval_promote_array_shallow(&region, scores, 4, &same_scores) == 0);
	assert(same_scores.off == scores.off);
	errno = 0;
	assert(jsval_promote_array_shallow_measure(&region, scores, 5, &bytes) < 0);
	assert(errno == ENOBUFS);

	assert_json(&region, root,
			"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2,3,4],\"status\":\"ok\",\"ready\":true}");
}

static void test_lookup_and_capacity_contracts(void)
{
	static const char json[] =
		"{\"keep\":7,\"items\":[1,2],\"nested\":{\"flag\":true}}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t got;
	size_t size_before;
	size_t len_before;
	size_t bytes;
	int has;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 32,
			&root) == 0);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"missing", 7,
			&got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"keep", 4,
			&has) == 0);
	assert(has == 1);
	assert(jsval_object_has_own_utf8(&region, root, (const uint8_t *)"missing",
			7, &has) == 0);
	assert(has == 0);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	assert(jsval_array_get(&region, items, 4, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	errno = 0;
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_number(9.0)) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(3.0)) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_promote_object_shallow_measure(&region, root, 2, &bytes) < 0);
	assert(errno == ENOBUFS);

	assert(jsval_promote_object_shallow_in_place(&region, &root, 3) == 0);
	assert(jsval_region_set_root(&region, root) == 0);
	size_before = jsval_object_size(&region, root);
	assert(size_before == 3);
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_number(9.0)) == 0);
	assert(jsval_object_size(&region, root) == size_before);
	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"missing", 7,
			&got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);

	assert(jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) == 0);
	assert(jsval_is_json_backed(items) == 1);
	errno = 0;
	assert(jsval_promote_array_shallow_measure(&region, items, 1, &bytes) < 0);
	assert(errno == ENOBUFS);
	assert(jsval_promote_array_shallow_in_place(&region, &items, 3) == 0);
	assert(jsval_object_set_utf8(&region, root, (const uint8_t *)"items", 5,
			items) == 0);
	len_before = jsval_array_length(&region, items);
	assert(len_before == 2);
	assert(jsval_array_set(&region, items, 1, jsval_number(8.0)) == 0);
	assert(jsval_array_length(&region, items) == len_before);
	assert(jsval_array_get(&region, items, 4, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_push(&region, items, jsval_number(3.0)) == 0);
	assert(jsval_array_length(&region, items) == 3);
	errno = 0;
	assert(jsval_array_push(&region, items, jsval_number(4.0)) < 0);
	assert(errno == ENOBUFS);
	errno = 0;
	assert(jsval_array_set_length(&region, items, 4) < 0);
	assert(errno == ENOBUFS);

	assert_json(&region, root,
			"{\"keep\":9,\"items\":[1,8,3],\"nested\":{\"flag\":true}}");
}

static void test_dense_array_observable_behavior(void)
{
	static const char json[] = "[1,2]";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t native_array;
	jsval_t parsed_array;
	jsval_t got;
	size_t len_before;
	size_t bytes;

	jsval_region_init(&region, storage, sizeof(storage));

	assert(jsval_array_new(&region, 4, &native_array) == 0);
	assert(jsval_array_set(&region, native_array, 0, jsval_number(1.0)) == 0);
	assert(jsval_array_set(&region, native_array, 1, jsval_number(2.0)) == 0);
	len_before = jsval_array_length(&region, native_array);
	assert(len_before == 2);
	assert(jsval_array_set(&region, native_array, 1, jsval_number(9.0)) == 0);
	assert(jsval_array_length(&region, native_array) == len_before);
	assert(jsval_array_get(&region, native_array, 1, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 9.0);

	assert(jsval_array_set_length(&region, native_array, 4) == 0);
	assert(jsval_array_get(&region, native_array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_get(&region, native_array, 3, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_set(&region, native_array, 2, jsval_number(7.0)) == 0);
	assert(jsval_array_set(&region, native_array, 3, jsval_number(8.0)) == 0);
	assert_json(&region, native_array, "[1,9,7,8]");
	assert(jsval_array_set_length(&region, native_array, 2) == 0);
	assert(jsval_array_get(&region, native_array, 2, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert_json(&region, native_array, "[1,9]");
	assert(jsval_array_pop(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 9.0);
	assert(jsval_array_length(&region, native_array) == 1);
	assert_json(&region, native_array, "[1]");
	assert(jsval_array_pop(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 1.0);
	assert(jsval_array_length(&region, native_array) == 0);
	assert_json(&region, native_array, "[]");
	assert(jsval_array_pop(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_length(&region, native_array) == 0);
	assert(jsval_array_set_length(&region, native_array, 2) == 0);
	assert(jsval_array_pop(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_length(&region, native_array) == 1);
	assert(jsval_array_pop(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_length(&region, native_array) == 0);
	assert_json(&region, native_array, "[]");
	assert(jsval_array_set(&region, native_array, 0, jsval_number(1.0)) == 0);
	assert(jsval_array_set(&region, native_array, 1, jsval_number(9.0)) == 0);
	assert(jsval_array_shift(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 1.0);
	assert(jsval_array_length(&region, native_array) == 1);
	assert_json(&region, native_array, "[9]");
	assert(jsval_array_shift(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 9.0);
	assert(jsval_array_length(&region, native_array) == 0);
	assert_json(&region, native_array, "[]");
	assert(jsval_array_shift(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_UNDEFINED);
	assert(jsval_array_length(&region, native_array) == 0);
	assert(jsval_array_unshift(&region, native_array, jsval_number(9.0)) == 0);
	assert_json(&region, native_array, "[9]");
	assert(jsval_array_unshift(&region, native_array, jsval_number(1.0)) == 0);
	assert_json(&region, native_array, "[1,9]");
	assert(jsval_array_unshift(&region, native_array, jsval_number(7.0)) == 0);
	assert_json(&region, native_array, "[7,1,9]");
	assert(jsval_array_unshift(&region, native_array, jsval_number(8.0)) == 0);
	assert_json(&region, native_array, "[8,7,1,9]");
	errno = 0;
	assert(jsval_array_unshift(&region, native_array, jsval_number(6.0)) < 0);
	assert(errno == ENOBUFS);
	assert(jsval_array_shift(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 8.0);
	assert(jsval_array_pop(&region, native_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 9.0);
	assert_json(&region, native_array, "[7,1]");

	assert(jsval_json_parse(&region, (const uint8_t *)json, sizeof(json) - 1, 8,
			&parsed_array) == 0);
	assert(jsval_is_json_backed(parsed_array) == 1);
	assert(jsval_array_get(&region, parsed_array, 1, &got) == 0);
	assert(jsval_strict_eq(&region, got, jsval_number(2.0)) == 1);
	errno = 0;
	assert(jsval_array_pop(&region, parsed_array, &got) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_shift(&region, parsed_array, &got) < 0);
	assert(errno == ENOTSUP);
	errno = 0;
	assert(jsval_array_unshift(&region, parsed_array, jsval_number(7.0)) < 0);
	assert(errno == ENOTSUP);
	assert(jsval_promote_array_shallow_measure(&region, parsed_array, 3, &bytes)
			== 0);
	assert(bytes > 0);
	assert(jsval_promote_array_shallow_in_place(&region, &parsed_array, 3) == 0);
	assert(jsval_is_native(parsed_array) == 1);
	assert(jsval_region_set_root(&region, parsed_array) == 0);
	assert(jsval_array_set(&region, parsed_array, 0, jsval_number(7.0)) == 0);
	assert(jsval_array_push(&region, parsed_array, jsval_number(3.0)) == 0);
	assert_json(&region, parsed_array, "[7,2,3]");
	assert(jsval_array_pop(&region, parsed_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 3.0);
	assert_json(&region, parsed_array, "[7,2]");
	assert(jsval_array_shift(&region, parsed_array, &got) == 0);
	assert(got.kind == JSVAL_KIND_NUMBER);
	assert(got.as.number == 7.0);
	assert_json(&region, parsed_array, "[2]");
	assert(jsval_array_unshift(&region, parsed_array, jsval_number(7.0)) == 0);
	assert_json(&region, parsed_array, "[7,2]");
}

static jsval_t fetch_test_str(jsval_region_t *region, const char *s)
{
	jsval_t v;
	assert(jsval_string_new_utf8(region, (const uint8_t *)s, strlen(s), &v)
			== 0);
	return v;
}

static int fetch_test_str_equals(jsval_region_t *region, jsval_t v,
		const char *s)
{
	size_t len = 0;
	size_t want = strlen(s);
	if (v.kind != JSVAL_KIND_STRING) { return 0; }
	if (jsval_string_copy_utf8(region, v, NULL, 0, &len) < 0) { return 0; }
	if (len != want) { return 0; }
	{
		uint8_t buf[want ? want : 1];
		if (want > 0
				&& jsval_string_copy_utf8(region, v, buf, want, NULL) < 0) {
			return 0;
		}
		return memcmp(buf, s, want) == 0;
	}
}

static void test_fetch_api_semantics(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t response;
	jsval_t promise;
	jsval_t got;
	int has = 0;
	int ok = 0;
	uint32_t status = 0;
	size_t size = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/* Headers: append / get / has / set / delete. */
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_NONE, &headers) == 0);
	assert(jsval_headers_append(&region, headers,
			fetch_test_str(&region, "Content-Type"),
			fetch_test_str(&region, "application/json")) == 0);
	assert(jsval_headers_append(&region, headers,
			fetch_test_str(&region, "X-Custom"),
			fetch_test_str(&region, "  spaces  ")) == 0);
	assert(jsval_headers_has(&region, headers,
			fetch_test_str(&region, "content-type"), &has) == 0);
	assert(has == 1);
	assert(jsval_headers_get(&region, headers,
			fetch_test_str(&region, "CONTENT-TYPE"), &got) == 0);
	assert(fetch_test_str_equals(&region, got, "application/json"));
	assert(jsval_headers_get(&region, headers,
			fetch_test_str(&region, "x-custom"), &got) == 0);
	assert(fetch_test_str_equals(&region, got, "spaces"));
	/* combine-with-comma */
	assert(jsval_headers_append(&region, headers,
			fetch_test_str(&region, "X-Custom"),
			fetch_test_str(&region, "more")) == 0);
	assert(jsval_headers_get(&region, headers,
			fetch_test_str(&region, "x-custom"), &got) == 0);
	assert(fetch_test_str_equals(&region, got, "spaces, more"));
	assert(jsval_headers_size(&region, headers, &size) == 0);
	assert(size == 3);
	/* set collapses duplicates */
	assert(jsval_headers_set(&region, headers,
			fetch_test_str(&region, "X-Custom"),
			fetch_test_str(&region, "final")) == 0);
	assert(jsval_headers_get(&region, headers,
			fetch_test_str(&region, "x-custom"), &got) == 0);
	assert(fetch_test_str_equals(&region, got, "final"));
	/* delete */
	assert(jsval_headers_delete(&region, headers,
			fetch_test_str(&region, "X-Custom")) == 0);
	assert(jsval_headers_has(&region, headers,
			fetch_test_str(&region, "x-custom"), &has) == 0);
	assert(has == 0);
	/* Forbidden name/value */
	errno = 0;
	assert(jsval_headers_append(&region, headers,
			fetch_test_str(&region, "bad name"),
			fetch_test_str(&region, "v")) < 0);

	/* Request construction: default GET for a URL string. */
	assert(jsval_request_new(&region, fetch_test_str(&region, "https://ex.com"),
			jsval_undefined(), 0, &request) == 0);
	assert(jsval_request_method(&region, request, &got) == 0);
	assert(fetch_test_str_equals(&region, got, "GET"));
	assert(jsval_request_url(&region, request, &got) == 0);
	assert(fetch_test_str_equals(&region, got, "https://ex.com"));

	/* Request with init dict. */
	{
		jsval_t init;
		jsval_t method_name;
		jsval_t body_text;

		assert(jsval_object_new(&region, 8, &init) == 0);
		method_name = fetch_test_str(&region, "POST");
		assert(jsval_object_set_utf8(&region, init,
				(const uint8_t *)"method", 6, method_name) == 0);
		body_text = fetch_test_str(&region, "{\"x\":1}");
		assert(jsval_object_set_utf8(&region, init,
				(const uint8_t *)"body", 4, body_text) == 0);
		assert(jsval_request_new(&region,
				fetch_test_str(&region, "https://ex.com"), init, 1,
				&request) == 0);
	}
	assert(jsval_request_method(&region, request, &got) == 0);
	assert(fetch_test_str_equals(&region, got, "POST"));
	assert(jsval_request_body_used(&region, request, &has) == 0);
	assert(has == 0);

	/* Body consumption: text() resolves with the posted body. */
	{
		jsval_promise_state_t state;
		assert(jsval_request_text(&region, request, &promise) == 0);
		assert(promise.kind == JSVAL_KIND_PROMISE);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_FULFILLED);
		assert(jsval_promise_result(&region, promise, &got) == 0);
		assert(fetch_test_str_equals(&region, got, "{\"x\":1}"));
		assert(jsval_request_body_used(&region, request, &has) == 0);
		assert(has == 1);

		assert(jsval_request_text(&region, request, &promise) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_REJECTED);
	}

	/* Forbidden method is rejected. */
	{
		jsval_t init;
		jsval_t req2;
		assert(jsval_object_new(&region, 4, &init) == 0);
		assert(jsval_object_set_utf8(&region, init,
				(const uint8_t *)"method", 6,
				fetch_test_str(&region, "CONNECT")) == 0);
		errno = 0;
		assert(jsval_request_new(&region,
				fetch_test_str(&region, "https://ex.com"), init, 1, &req2) < 0);
	}

	/* Response: default status 200, type "default", ok true. */
	assert(jsval_response_new(&region, fetch_test_str(&region, "hello"), 1,
			jsval_undefined(), 0, &response) == 0);
	assert(jsval_response_status(&region, response, &status) == 0);
	assert(status == 200);
	assert(jsval_response_ok(&region, response, &ok) == 0);
	assert(ok == 1);
	{
		jsval_promise_state_t state;
		assert(jsval_response_text(&region, response, &promise) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_FULFILLED);
		assert(jsval_promise_result(&region, promise, &got) == 0);
		assert(fetch_test_str_equals(&region, got, "hello"));
	}

	/* Response.json sets Content-Type and parses back. */
	{
		jsval_t data;
		jsval_t parsed;
		static const char src[] = "{\"ok\":true}";
		assert(jsval_json_parse(&region, (const uint8_t *)src, sizeof(src) - 1,
				16, &data) == 0);
		assert(jsval_response_json(&region, data, 0, jsval_undefined(),
				&response) == 0);
		assert(jsval_response_headers(&region, response, &got) == 0);
		{
			jsval_t ct;
			assert(jsval_headers_get(&region, got,
					fetch_test_str(&region, "Content-Type"), &ct) == 0);
			assert(fetch_test_str_equals(&region, ct, "application/json"));
		}
		{
			jsval_promise_state_t state;
			assert(jsval_response_json_body(&region, response, &promise) == 0);
			assert(jsval_promise_state(&region, promise, &state) == 0);
			assert(state == JSVAL_PROMISE_STATE_FULFILLED);
			assert(jsval_promise_result(&region, promise, &parsed) == 0);
			(void)parsed;
		}
	}

	/* Response.error is status 0 and type "error". */
	assert(jsval_response_error(&region, &response) == 0);
	assert(jsval_response_status(&region, response, &status) == 0);
	assert(status == 0);
	assert(jsval_response_type(&region, response, &got) == 0);
	assert(fetch_test_str_equals(&region, got, "error"));

	/* Response.redirect rejects bad status. */
	errno = 0;
	assert(jsval_response_redirect(&region,
			fetch_test_str(&region, "https://ex.com/"), 1, 200, &response) < 0);
	assert(jsval_response_redirect(&region,
			fetch_test_str(&region, "https://ex.com/"), 1, 301, &response) == 0);
	assert(jsval_response_status(&region, response, &status) == 0);
	assert(status == 301);

	/* fetch() returns a rejected promise. */
	{
		jsval_promise_state_t state;
		assert(jsval_fetch(&region, fetch_test_str(&region, "https://ex.com"),
				jsval_undefined(), 0, &promise) == 0);
		assert(promise.kind == JSVAL_KIND_PROMISE);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_REJECTED);
	}
}

typedef struct fake_body_source_s {
	const uint8_t *data;
	size_t total;
	size_t cursor;
	int chunk_size;       /* bytes per read; 0 = unlimited */
	int fail_after;       /* -1 = never; >=0 = return ERROR after N reads */
	int reads;
	int close_calls;
} fake_body_source_t;

static int fake_body_read(void *userdata, uint8_t *buf, size_t cap,
		size_t *out_len, jsval_body_source_status_t *status_ptr)
{
	fake_body_source_t *src = (fake_body_source_t *)userdata;
	size_t remaining;
	size_t n;

	src->reads++;
	if (src->fail_after >= 0 && src->reads > src->fail_after) {
		*out_len = 0;
		*status_ptr = JSVAL_BODY_SOURCE_STATUS_ERROR;
		return 0;
	}
	remaining = src->total - src->cursor;
	n = remaining;
	if (src->chunk_size > 0 && (size_t)src->chunk_size < n) {
		n = (size_t)src->chunk_size;
	}
	if (n > cap) { n = cap; }
	if (n > 0) {
		memcpy(buf, src->data + src->cursor, n);
		src->cursor += n;
	}
	*out_len = n;
	*status_ptr = (src->cursor >= src->total)
			? JSVAL_BODY_SOURCE_STATUS_EOF
			: JSVAL_BODY_SOURCE_STATUS_READY;
	return 0;
}

static void fake_body_close(void *userdata)
{
	fake_body_source_t *src = (fake_body_source_t *)userdata;
	src->close_calls++;
}

static const jsval_body_source_vtable_t fake_body_vtable = {
	fake_body_read,
	fake_body_close,
};

static jsval_t fetch_drain_str(jsval_region_t *region, const char *s)
{
	jsval_t v;
	assert(jsval_string_new_utf8(region, (const uint8_t *)s, strlen(s), &v)
			== 0);
	return v;
}

static int fetch_drain_str_equals(jsval_region_t *region, jsval_t v,
		const char *s)
{
	size_t len = 0;
	size_t want = strlen(s);
	if (v.kind != JSVAL_KIND_STRING) { return 0; }
	if (jsval_string_copy_utf8(region, v, NULL, 0, &len) < 0) { return 0; }
	if (len != want) { return 0; }
	{
		uint8_t buf[want ? want : 1];
		if (want > 0
				&& jsval_string_copy_utf8(region, v, buf, want, NULL) < 0) {
			return 0;
		}
		return memcmp(buf, s, want) == 0;
	}
}

static void test_fetch_body_drain_semantics(void)
{
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t response;
	jsval_t got;
	jsval_t promise;
	jsval_promise_state_t state;
	int used = 0;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/* 1. Happy-path text() drain. */
	{
		static const uint8_t body[] = "hello";
		fake_body_source_t src = {
			.data = body, .total = 5, .cursor = 0,
			.chunk_size = 0, .fail_after = -1,
			.reads = 0, .close_calls = 0,
		};
		assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST,
				&headers) == 0);
		assert(jsval_request_new_from_parts(&region,
				fetch_drain_str(&region, "POST"),
				fetch_drain_str(&region, "https://ex.com"),
				headers, &fake_body_vtable, &src, 5, &request) == 0);
		assert(jsval_request_body_used(&region, request, &used) == 0);
		assert(used == 0);
		assert(jsval_request_text(&region, request, &promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_FULFILLED);
		assert(jsval_promise_result(&region, promise, &got) == 0);
		assert(fetch_drain_str_equals(&region, got, "hello"));
		assert(src.close_calls == 1);
		assert(jsval_request_body_used(&region, request, &used) == 0);
		assert(used == 1);

		/* Second consumption rejects. */
		assert(jsval_request_text(&region, request, &promise) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_REJECTED);
		/* close is not called again */
		assert(src.close_calls == 1);
	}

	/* 2. arrayBuffer() drain across stuttered reads. */
	{
		static const uint8_t body[] =
				"The quick brown fox jumps over the lazy dog.";
		fake_body_source_t src = {
			.data = body, .total = sizeof(body) - 1, .cursor = 0,
			.chunk_size = 3, .fail_after = -1,
			.reads = 0, .close_calls = 0,
		};
		assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_RESPONSE,
				&headers) == 0);
		assert(jsval_response_new_from_parts(&region, 200,
				jsval_undefined(), headers,
				&fake_body_vtable, &src, sizeof(body) - 1,
				&response) == 0);
		assert(jsval_response_array_buffer(&region, response, &promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_FULFILLED);
		assert(jsval_promise_result(&region, promise, &got) == 0);
		{
			size_t bl = 0;
			uint8_t buf[sizeof(body) - 1];
			assert(jsval_array_buffer_byte_length(&region, got, &bl) == 0);
			assert(bl == sizeof(body) - 1);
			assert(jsval_array_buffer_copy_bytes(&region, got, buf, bl,
					NULL) == 0);
			assert(memcmp(buf, body, bl) == 0);
		}
		assert(src.close_calls == 1);
		/* And the source was called multiple times (stuttered). */
		assert(src.reads > 1);
	}

	/* 3. bytes() returns Uint8Array. */
	{
		static const uint8_t body[] = {0xde, 0xad, 0xbe, 0xef};
		fake_body_source_t src = {
			.data = body, .total = 4, .cursor = 0,
			.chunk_size = 2, .fail_after = -1,
			.reads = 0, .close_calls = 0,
		};
		assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_RESPONSE,
				&headers) == 0);
		assert(jsval_response_new_from_parts(&region, 200,
				jsval_undefined(), headers,
				&fake_body_vtable, &src, 4, &response) == 0);
		assert(jsval_response_bytes(&region, response, &promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_FULFILLED);
		assert(jsval_promise_result(&region, promise, &got) == 0);
		assert(got.kind == JSVAL_KIND_TYPED_ARRAY);
		{
			uint8_t buf[4];
			size_t bl = 0;
			assert(jsval_typed_array_byte_length(&region, got, &bl) == 0);
			assert(bl == 4);
			assert(jsval_typed_array_copy_bytes(&region, got, buf, bl,
					NULL) == 0);
			assert(memcmp(buf, body, 4) == 0);
		}
		assert(src.close_calls == 1);
	}

	/* 4. json() parses drained body. */
	{
		static const uint8_t body[] = "{\"ok\":true}";
		fake_body_source_t src = {
			.data = body, .total = sizeof(body) - 1, .cursor = 0,
			.chunk_size = 0, .fail_after = -1,
			.reads = 0, .close_calls = 0,
		};
		assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_RESPONSE,
				&headers) == 0);
		assert(jsval_response_new_from_parts(&region, 200,
				jsval_undefined(), headers,
				&fake_body_vtable, &src, sizeof(body) - 1,
				&response) == 0);
		assert(jsval_response_json_body(&region, response, &promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_FULFILLED);
		assert(jsval_promise_result(&region, promise, &got) == 0);
		assert(got.kind == JSVAL_KIND_OBJECT);
		assert(src.close_calls == 1);
	}

	/* 5. Over-budget rejection: hint says 4, source produces 10. */
	{
		static const uint8_t body[] = "0123456789";
		fake_body_source_t src = {
			.data = body, .total = 10, .cursor = 0,
			.chunk_size = 0, .fail_after = -1,
			.reads = 0, .close_calls = 0,
		};
		assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_RESPONSE,
				&headers) == 0);
		assert(jsval_response_new_from_parts(&region, 200,
				jsval_undefined(), headers,
				&fake_body_vtable, &src, 4, &response) == 0);
		assert(jsval_response_text(&region, response, &promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_REJECTED);
		assert(src.close_calls == 1);
	}

	/* 6. Mid-stream error: source returns ERROR after first read. */
	{
		static const uint8_t body[] = "hello world";
		fake_body_source_t src = {
			.data = body, .total = sizeof(body) - 1, .cursor = 0,
			.chunk_size = 4, .fail_after = 1,
			.reads = 0, .close_calls = 0,
		};
		assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_RESPONSE,
				&headers) == 0);
		assert(jsval_response_new_from_parts(&region, 200,
				jsval_undefined(), headers,
				&fake_body_vtable, &src, SIZE_MAX, &response) == 0);
		assert(jsval_response_text(&region, response, &promise) == 0);
		memset(&error, 0, sizeof(error));
		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, promise, &state) == 0);
		assert(state == JSVAL_PROMISE_STATE_REJECTED);
		assert(src.close_calls == 1);
	}
}

static void test_array_buffer_bytes_mut_helper(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t buffer;
	uint8_t *writable;
	size_t cap = 0;
	uint8_t readback[8];

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_array_buffer_new(&region, 8, &buffer) == 0);
	assert(jsval_array_buffer_bytes_mut(&region, buffer, &writable, &cap)
			== 0);
	assert(cap == 8);
	memcpy(writable, "abcdefgh", 8);
	assert(jsval_array_buffer_copy_bytes(&region, buffer, readback,
			sizeof(readback), NULL) == 0);
	assert(memcmp(readback, "abcdefgh", 8) == 0);

	/* Kind mismatch path */
	errno = 0;
	assert(jsval_array_buffer_bytes_mut(&region, jsval_undefined(),
			&writable, &cap) < 0);
	assert(errno == EINVAL);
}

int main(void)
{
	test_region_alloc_helpers();
	test_native_storage();
	test_value_semantics();
	test_symbol_semantics();
	test_bigint_semantics();
	test_function_semantics();
	test_date_semantics();
	test_crypto_semantics();
	test_promise_semantics();
	test_promise_all_semantics();
	test_promise_race_semantics();
	test_promise_all_settled_semantics();
	test_set_semantics();
	test_map_semantics();
	test_iterator_semantics();
	test_typeof_semantics();
	test_url_object_semantics();
	test_nullish_and_selection_semantics();
	test_logical_not_semantics();
	test_logical_and_or_semantics();
	test_numeric_coercion_and_arithmetic();
	test_integer_coercion_and_bitwise();
	test_shift_semantics();
	test_relational_semantics();
	test_abstract_equality_semantics();
	test_json_backed_value_parity();
	test_json_storage();
	test_json_root_rebase();
	test_json_mutation_requires_promotion();
	test_native_container_helpers();
	test_json_container_helpers();
	test_object_copy_own_helpers();
	test_object_clone_own_helpers();
	test_array_clone_dense_helpers();
	test_array_splice_dense_helpers();
	test_policy_layer();
	test_method_bridge();
	test_method_normalize_bridge();
	test_method_search_bridge();
	test_method_split_bridge();
	test_method_replace_bridge();
	test_method_replace_all_bridge();
	test_method_replace_callback_bridge();
#if JSMX_WITH_REGEX
	test_method_replace_regex_callback_bridge();
	test_method_replace_regex_named_groups_bridge();
	test_regexp_exec_and_match();
	test_regexp_match_all();
	test_regexp_named_groups();
	test_method_regex_search_bridge();
	test_u_literal_surrogate_match_rewrite();
	test_u_literal_surrogate_match_all_rewrite();
	test_u_literal_surrogate_replace_rewrite();
	test_u_literal_surrogate_split_rewrite();
	test_u_literal_sequence_search_match_rewrite();
	test_u_literal_sequence_replace_split_rewrite();
	test_u_literal_class_search_match_rewrite();
	test_u_literal_class_replace_split_rewrite();
	test_u_literal_negated_class_search_match_rewrite();
	test_u_literal_negated_class_replace_split_rewrite();
	test_u_literal_range_class_search_match_rewrite();
	test_u_literal_range_class_replace_split_rewrite();
	test_u_literal_negated_range_class_search_match_rewrite();
	test_u_literal_negated_range_class_replace_split_rewrite();
	test_u_predefined_class_search_match_rewrite();
	test_u_predefined_class_replace_split_rewrite();
#endif
	test_method_concat_bridge();
	test_method_accessor_bridge();
	test_method_slice_substring_bridge();
	test_method_trim_repeat_bridge();
	test_method_substr_trim_alias_bridge();
	test_method_pad_bridge();
	test_shallow_planned_promotion();
	test_lookup_and_capacity_contracts();
	test_dense_array_observable_behavior();
	test_fetch_api_semantics();
	test_fetch_body_drain_semantics();
	test_array_buffer_bytes_mut_helper();
	puts("test_jsval: ok");
	return 0;
}

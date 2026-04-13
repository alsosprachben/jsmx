#include <math.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "jsstr.h"
#include "jsval.h"

typedef enum generated_status_e {
	GENERATED_PASS = 0,
	GENERATED_KNOWN_UNSUPPORTED = 1,
	GENERATED_WRONG_RESULT = 2
} generated_status_t;

typedef generated_status_t (*generated_case_fn)(char *detail, size_t cap);

typedef struct generated_case_s {
	const char *suite;
	const char *name;
	generated_case_fn run;
} generated_case_t;

typedef struct generated_callback_ctx_s {
	int should_throw;
	const uint16_t *text;
	size_t len;
	int *calls_ptr;
} generated_callback_ctx_t;

typedef struct generated_replace_callback_ctx_s {
	int call_count;
	int should_throw;
} generated_replace_callback_ctx_t;

typedef struct generated_regex_replace_callback_ctx_s {
	int call_count;
	int should_throw;
} generated_regex_replace_callback_ctx_t;

typedef struct generated_promise_identity_state_s {
	int call_count;
	size_t last_argc;
	jsval_t last_arg;
} generated_promise_identity_state_t;

typedef struct generated_promise_return_state_s {
	int call_count;
	size_t last_argc;
	jsval_t last_arg;
	jsval_t return_value;
} generated_promise_return_state_t;

typedef struct generated_promise_throw_state_s {
	int call_count;
} generated_promise_throw_state_t;

typedef struct generated_microtask_state_s {
	int call_count;
	int enqueue_nested;
	jsval_t nested_function;
} generated_microtask_state_t;

typedef struct generated_scheduler_state_s {
	int enqueue_count;
	int wake_count;
} generated_scheduler_state_t;

static generated_promise_identity_state_t generated_promise_identity_state;
static generated_promise_return_state_t generated_promise_return_state;
static generated_promise_throw_state_t generated_promise_throw_state;
static generated_microtask_state_t generated_microtask_state;

static generated_status_t generated_write_detail(generated_status_t status, char *detail, size_t cap, const char *fmt, ...)
{
	va_list ap;

	if (detail != NULL && cap > 0) {
		va_start(ap, fmt);
		vsnprintf(detail, cap, fmt, ap);
		va_end(ap);
	}
	return status;
}

static generated_status_t generated_failf(char *detail, size_t cap, const char *fmt, ...)
{
	va_list ap;

	if (detail != NULL && cap > 0) {
		va_start(ap, fmt);
		vsnprintf(detail, cap, fmt, ap);
		va_end(ap);
	}
	return GENERATED_WRONG_RESULT;
}

static generated_status_t generated_fail_errno(char *detail, size_t cap, const char *context)
{
	int err = errno;
	return generated_failf(detail, cap, "%s failed: %s", context, strerror(err));
}

static generated_status_t generated_expect_jsstr8(jsstr8_t *value, const uint8_t *expected, size_t expected_len, char *detail, size_t cap)
{
	if (value->len != expected_len) {
		return generated_failf(detail, cap, "expected %zu bytes, got %zu", expected_len, value->len);
	}
	if (memcmp(value->bytes, expected, expected_len) != 0) {
		return generated_failf(detail, cap, "string bytes did not match expected output");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_json(jsval_region_t *region, jsval_t value, const uint8_t *expected, size_t expected_len, char *detail, size_t cap)
{
	size_t actual_len = 0;
	uint8_t actual_buf[expected_len ? expected_len : 1];

	if (jsval_copy_json(region, value, NULL, 0, &actual_len) < 0) {
		return generated_fail_errno(detail, cap, "jsval_copy_json(length)");
	}
	if (actual_len != expected_len) {
		return generated_failf(detail, cap, "expected %zu JSON bytes, got %zu", expected_len, actual_len);
	}
	if (jsval_copy_json(region, value, actual_buf, actual_len, NULL) < 0) {
		return generated_fail_errno(detail, cap, "jsval_copy_json(copy)");
	}
	if (memcmp(actual_buf, expected, expected_len) != 0) {
		return generated_failf(detail, cap, "emitted JSON did not match expected output");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_string(jsval_region_t *region,
		jsval_t value, const uint8_t *expected, size_t expected_len,
		char *detail, size_t cap)
{
	size_t actual_len = 0;
	uint8_t actual_buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_copy_utf8(length)");
	}
	if (actual_len != expected_len) {
		return generated_failf(detail, cap, "expected %zu string bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, actual_buf, actual_len, NULL) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_copy_utf8(copy)");
	}
	if (memcmp(actual_buf, expected, expected_len) != 0) {
		return generated_failf(detail, cap, "string bytes did not match expected output");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_array_buffer_bytes(
		jsval_region_t *region, jsval_t value, const uint8_t *expected,
		size_t expected_len, char *detail, size_t cap)
{
	size_t actual_len = 0;
	uint8_t actual_buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_ARRAY_BUFFER) {
		return generated_failf(detail, cap, "expected ArrayBuffer result");
	}
	if (jsval_array_buffer_copy_bytes(region, value, NULL, 0, &actual_len) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_buffer_copy_bytes(length)");
	}
	if (actual_len != expected_len) {
		return generated_failf(detail, cap,
				"expected %zu array-buffer bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_array_buffer_copy_bytes(region, value, actual_buf, actual_len,
			NULL) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_buffer_copy_bytes(copy)");
	}
	if (memcmp(actual_buf, expected, expected_len) != 0) {
		return generated_failf(detail, cap,
				"array-buffer bytes did not match expected output");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_bigint(jsval_region_t *region,
		jsval_t value, const uint8_t *expected, size_t expected_len,
		char *detail, size_t cap)
{
	size_t actual_len = 0;
	uint8_t actual_buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_BIGINT) {
		return generated_failf(detail, cap, "expected bigint result");
	}
	if (jsval_bigint_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bigint_copy_utf8(length)");
	}
	if (actual_len != expected_len) {
		return generated_failf(detail, cap,
				"expected %zu bigint bytes, got %zu",
				expected_len, actual_len);
	}
	if (jsval_bigint_copy_utf8(region, value, actual_buf, actual_len, NULL) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bigint_copy_utf8(copy)");
	}
	if (memcmp(actual_buf, expected, expected_len) != 0) {
		return generated_failf(detail, cap,
				"bigint bytes did not match expected output");
	}
	return GENERATED_PASS;
}

static generated_status_t
generated_expect_utf16_string(jsval_region_t *region, jsval_t value,
		const uint16_t *expected, size_t expected_len,
		char *detail, size_t cap)
{
	static const uint16_t empty_unit = 0;
	jsval_t expected_value;

	if (value.kind != JSVAL_KIND_STRING) {
		return generated_failf(detail, cap, "expected string result");
	}
	if (jsval_string_new_utf16(region,
			expected_len > 0 ? expected : &empty_unit, expected_len,
			&expected_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf16");
	}
	if (jsval_strict_eq(region, value, expected_value) != 1) {
		return generated_failf(detail, cap,
				"utf16 string result did not match expected output");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_string_array(jsval_region_t *region,
		jsval_t array, const char *const *expected, size_t expected_len,
		char *detail, size_t cap)
{
	size_t i;

	if (array.kind != JSVAL_KIND_ARRAY) {
		return generated_failf(detail, cap, "expected array result");
	}
	if (jsval_array_length(region, array) != expected_len) {
		return generated_failf(detail, cap, "expected array length %zu, got %zu",
				expected_len, jsval_array_length(region, array));
	}
	for (i = 0; i < expected_len; i++) {
		jsval_t value;

		if (jsval_array_get(region, array, i, &value) < 0) {
			return generated_fail_errno(detail, cap, "jsval_array_get");
		}
		if (expected[i] == NULL) {
			if (value.kind != JSVAL_KIND_UNDEFINED) {
				return generated_failf(detail, cap,
						"expected undefined array element at %zu", i);
			}
		} else {
			generated_status_t status =
				generated_expect_string(region, value,
						(const uint8_t *)expected[i], strlen(expected[i]),
						detail, cap);

			if (status != GENERATED_PASS) {
				return status;
			}
		}
	}
	return GENERATED_PASS;
}

static int
generated_build_object_keys_array(jsval_region_t *region, jsval_t object,
		jsval_t *array_ptr)
{
	jsval_t array;
	size_t len;
	size_t i;

	if (region == NULL || array_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_object_size(region, object);
	if (jsval_array_new(region, len, &array) < 0) {
		return -1;
	}
	for (i = 0; i < len; i++) {
		jsval_t key;

		if (jsval_object_key_at(region, object, i, &key) < 0) {
			return -1;
		}
		if (jsval_array_push(region, array, key) < 0) {
			return -1;
		}
	}
	*array_ptr = array;
	return 0;
}

static int
generated_build_object_values_array(jsval_region_t *region, jsval_t object,
		jsval_t *array_ptr)
{
	jsval_t array;
	size_t len;
	size_t i;

	if (region == NULL || array_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_object_size(region, object);
	if (jsval_array_new(region, len, &array) < 0) {
		return -1;
	}
	for (i = 0; i < len; i++) {
		jsval_t value;

		if (jsval_object_value_at(region, object, i, &value) < 0) {
			return -1;
		}
		if (jsval_array_push(region, array, value) < 0) {
			return -1;
		}
	}
	*array_ptr = array;
	return 0;
}

static int
generated_build_object_entries_array(jsval_region_t *region, jsval_t object,
		jsval_t *array_ptr)
{
	jsval_t array;
	size_t len;
	size_t i;

	if (region == NULL || array_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_object_size(region, object);
	if (jsval_array_new(region, len, &array) < 0) {
		return -1;
	}
	for (i = 0; i < len; i++) {
		jsval_t pair;
		jsval_t key;
		jsval_t value;

		if (jsval_array_new(region, 2, &pair) < 0) {
			return -1;
		}
		if (jsval_object_key_at(region, object, i, &key) < 0) {
			return -1;
		}
		if (jsval_object_value_at(region, object, i, &value) < 0) {
			return -1;
		}
		if (jsval_array_push(region, pair, key) < 0
				|| jsval_array_push(region, pair, value) < 0
				|| jsval_array_push(region, array, pair) < 0) {
			return -1;
		}
	}
	*array_ptr = array;
	return 0;
}

static int
generated_append_dense_array(jsval_region_t *region, jsval_t dst, jsval_t src)
{
	size_t len;
	size_t i;

	if (region == NULL) {
		errno = EINVAL;
		return -1;
	}
	len = jsval_array_length(region, src);
	for (i = 0; i < len; i++) {
		jsval_t value;

		if (jsval_array_get(region, src, i, &value) < 0) {
			return -1;
		}
		if (jsval_array_push(region, dst, value) < 0) {
			return -1;
		}
	}
	return 0;
}

static generated_status_t generated_expect_number(jsval_region_t *region, jsval_t value, double expected,
		char *detail, size_t cap)
{
	if (value.kind != JSVAL_KIND_NUMBER) {
		return generated_failf(detail, cap, "expected numeric result");
	}
	if (jsval_strict_eq(region, value, jsval_number(expected)) != 1) {
		return generated_failf(detail, cap, "expected numeric result %.17g",
				expected);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_positive_zero(jsval_t value,
		char *detail, size_t cap)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != 0.0
			|| signbit(value.as.number)) {
		return generated_failf(detail, cap,
				"expected numeric +0 result");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_boolean_result(int actual,
		int expected, char *detail, size_t cap)
{
	if (!!actual != !!expected) {
		return generated_failf(detail, cap, "expected %s comparison result",
				expected ? "true" : "false");
	}
	return GENERATED_PASS;
}

static int
generated_callback_to_string(void *opaque, jsstr16_t *out,
		jsmethod_error_t *error)
{
	generated_callback_ctx_t *ctx = (generated_callback_ctx_t *)opaque;

	if (ctx->calls_ptr != NULL) {
		(*ctx->calls_ptr)++;
	}
	if (ctx->should_throw) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "generated callback threw";
		return -1;
	}
	if (ctx->text != NULL &&
			jsstr16_set_from_utf16(out, ctx->text, ctx->len) != ctx->len) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

static int
generated_type_error_to_string(void *opaque, jsstr16_t *out,
		jsmethod_error_t *error)
{
	(void)opaque;
	(void)out;
	error->kind = JSMETHOD_ERROR_TYPE;
	error->message = "TypeError";
	return -1;
}

static int
generated_copy_jsval_utf8(jsval_region_t *region, jsval_t value, char *buf,
		size_t cap)
{
	size_t len = 0;

	if (region == NULL || buf == NULL || cap == 0) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return -1;
	}
	if (len + 1 > cap) {
		errno = ENOBUFS;
		return -1;
	}
	if (len > 0 && jsval_string_copy_utf8(region, value, (uint8_t *)buf, len,
			NULL) < 0) {
		return -1;
	}
	buf[len] = '\0';
	return 0;
}

static int
generated_replace_offset_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_replace_callback_ctx_t *ctx =
			(generated_replace_callback_ctx_t *)opaque;
	char buf[32];
	int len;

	(void)error;
	if (ctx != NULL) {
		ctx->call_count++;
		if (ctx->should_throw) {
			errno = EINVAL;
			error->kind = JSMETHOD_ERROR_ABRUPT;
			error->message = "generated replace callback threw";
			return -1;
		}
	}
	if (call->groups.kind != JSVAL_KIND_UNDEFINED) {
		errno = EINVAL;
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "[%zu]", call->offset);
	if (len < 0 || (size_t)len >= sizeof(buf)) {
		errno = EOVERFLOW;
		return -1;
	}
	return jsval_string_new_utf8(region, (const uint8_t *)buf, (size_t)len,
			result_ptr);
}

#if JSMX_WITH_REGEX
static int
generated_regex_replace_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_regex_replace_callback_ctx_t *ctx =
			(generated_regex_replace_callback_ctx_t *)opaque;
	char match_buf[16];
	char cap1_buf[16];
	char cap2_buf[16];
	char buf[96];
	int len;

	(void)error;
	if (ctx != NULL) {
		ctx->call_count++;
		if (ctx->should_throw) {
			errno = EINVAL;
			error->kind = JSMETHOD_ERROR_ABRUPT;
			error->message = "generated regex replace callback threw";
			return -1;
		}
	}
	if (generated_copy_jsval_utf8(region, call->match, match_buf,
			sizeof(match_buf)) < 0) {
		return -1;
	}
	if (call->capture_count < 2 || call->captures == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (call->groups.kind != JSVAL_KIND_UNDEFINED) {
		errno = EINVAL;
		return -1;
	}
	if (generated_copy_jsval_utf8(region, call->captures[0], cap1_buf,
			sizeof(cap1_buf)) < 0) {
		return -1;
	}
	if (call->captures[1].kind == JSVAL_KIND_UNDEFINED) {
		strcpy(cap2_buf, "U");
	} else if (generated_copy_jsval_utf8(region, call->captures[1], cap2_buf,
			sizeof(cap2_buf)) < 0) {
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "<%s|%s|%s|%zu>", match_buf, cap1_buf,
			cap2_buf, call->offset);
	if (len < 0 || (size_t)len >= sizeof(buf)) {
		errno = EOVERFLOW;
		return -1;
	}
	return jsval_string_new_utf8(region, (const uint8_t *)buf, (size_t)len,
			result_ptr);
}

static int
generated_named_groups_replace_callback(jsval_region_t *region, void *opaque,
		const jsval_replace_call_t *call, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	generated_regex_replace_callback_ctx_t *ctx =
			(generated_regex_replace_callback_ctx_t *)opaque;
	jsval_t value;
	char digits_buf[16];
	char tail_buf[16];
	char buf[96];
	int len;

	(void)error;
	if (ctx != NULL) {
		ctx->call_count++;
		if (ctx->should_throw) {
			errno = EINVAL;
			error->kind = JSMETHOD_ERROR_ABRUPT;
			error->message = "generated named-group replace callback threw";
			return -1;
		}
	}
	if (call->groups.kind != JSVAL_KIND_OBJECT) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_object_get_utf8(region, call->groups,
			(const uint8_t *)"digits", 6, &value) < 0
			|| generated_copy_jsval_utf8(region, value, digits_buf,
				sizeof(digits_buf)) < 0) {
		return -1;
	}
	if (jsval_object_get_utf8(region, call->groups,
			(const uint8_t *)"tail", 4, &value) < 0) {
		return -1;
	}
	if (value.kind == JSVAL_KIND_UNDEFINED) {
		strcpy(tail_buf, "U");
	} else if (generated_copy_jsval_utf8(region, value, tail_buf,
			sizeof(tail_buf)) < 0) {
		return -1;
	}
	len = snprintf(buf, sizeof(buf), "<%s|%s|%zu>", digits_buf, tail_buf,
			call->offset);
	if (len < 0 || (size_t)len >= sizeof(buf)) {
		errno = EOVERFLOW;
		return -1;
	}
	return jsval_string_new_utf8(region, (const uint8_t *)buf, (size_t)len,
			result_ptr);
}
#endif

static int
generated_static_sum_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)error;
	if (region == NULL || result_ptr == NULL || argc < 2 || argv == NULL) {
		errno = EINVAL;
		return -1;
	}
	return jsval_add(region, argv[0], argv[1], result_ptr);
}

static int
generated_static_echo_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)error;
	if (result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	*result_ptr = (argc > 0 && argv != NULL) ? argv[0] : jsval_undefined();
	return 0;
}

static int
generated_static_throw_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)argc;
	(void)argv;
	(void)result_ptr;
	errno = EINVAL;
	if (error != NULL) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "generated function threw";
	}
	return -1;
}

static int
generated_promise_identity_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)error;
	if (result_ptr == NULL || (argc > 0 && argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
	generated_promise_identity_state.call_count++;
	generated_promise_identity_state.last_argc = argc;
	generated_promise_identity_state.last_arg =
		(argc > 0) ? argv[0] : jsval_undefined();
	*result_ptr = (argc > 0) ? argv[0] : jsval_undefined();
	return 0;
}

static int
generated_promise_return_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)error;
	if (result_ptr == NULL || (argc > 0 && argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
	generated_promise_return_state.call_count++;
	generated_promise_return_state.last_argc = argc;
	generated_promise_return_state.last_arg =
		(argc > 0) ? argv[0] : jsval_undefined();
	*result_ptr = generated_promise_return_state.return_value;
	return 0;
}

static int
generated_promise_throw_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)region;
	(void)argc;
	(void)argv;
	(void)result_ptr;
	generated_promise_throw_state.call_count++;
	errno = EINVAL;
	if (error != NULL) {
		error->kind = JSMETHOD_ERROR_ABRUPT;
		error->message = "generated promise handler threw";
	}
	return -1;
}

static int
generated_microtask_function(jsval_region_t *region, size_t argc,
		const jsval_t *argv, jsval_t *result_ptr, jsmethod_error_t *error)
{
	(void)error;
	if (region == NULL || result_ptr == NULL || (argc > 0 && argv == NULL)) {
		errno = EINVAL;
		return -1;
	}
	generated_microtask_state.call_count++;
	if (generated_microtask_state.enqueue_nested
			&& generated_microtask_state.nested_function.kind
				== JSVAL_KIND_FUNCTION) {
		generated_microtask_state.enqueue_nested = 0;
		if (jsval_microtask_enqueue(region,
				generated_microtask_state.nested_function, 0, NULL) < 0) {
			return -1;
		}
	}
	*result_ptr = (argc > 0) ? argv[0] : jsval_undefined();
	return 0;
}

static void
generated_scheduler_on_enqueue(jsval_region_t *region, void *ctx)
{
	(void)region;
	if (ctx != NULL) {
		((generated_scheduler_state_t *)ctx)->enqueue_count++;
	}
}

static void
generated_scheduler_on_wake(jsval_region_t *region, void *ctx)
{
	(void)region;
	if (ctx != NULL) {
		((generated_scheduler_state_t *)ctx)->wake_count++;
	}
}

static generated_status_t generated_expect_negative_zero(jsval_t value,
		char *detail, size_t cap)
{
	if (value.kind != JSVAL_KIND_NUMBER || value.as.number != 0.0
			|| !signbit(value.as.number)) {
		return generated_failf(detail, cap,
				"expected numeric -0 result");
	}
	return GENERATED_PASS;
}

static generated_status_t generated_expect_nan_value(jsval_t value,
		char *detail, size_t cap)
{
	if (value.kind != JSVAL_KIND_NUMBER || !isnan(value.as.number)) {
		return generated_failf(detail, cap,
				"expected NaN numeric result");
	}
	return GENERATED_PASS;
}

static jsval_t generated_logical_and(jsval_region_t *region, jsval_t left,
		jsval_t right)
{
	return jsval_truthy(region, left) ? right : left;
}

static jsval_t generated_logical_or(jsval_region_t *region, jsval_t left,
		jsval_t right)
{
	return jsval_truthy(region, left) ? left : right;
}

static generated_status_t generated_smoke_json_promote_emit(char *detail, size_t cap)
{
	static const uint8_t input[] =
		"{\"profile\":{\"name\":\"Ada\",\"active\":true},\"scores\":[1,2,3],\"note\":\"hi\"}";
	static const uint8_t expected[] =
		"{\"profile\":{\"name\":\"Ada\",\"active\":false},\"scores\":[7,2,3],\"note\":\"updated\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t profile;
	jsval_t name;
	jsval_t active;
	jsval_t scores;
	jsval_t second_score;
	jsval_t replacement;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 32, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (!jsval_is_json_backed(root)) {
		return generated_failf(detail, cap, "expected parsed root to stay JSON-backed");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7, &profile) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(profile)");
	}
	if (!jsval_is_json_backed(profile)) {
		return generated_failf(detail, cap, "expected nested profile to stay JSON-backed");
	}
	if (jsval_object_get_utf8(&region, profile, (const uint8_t *)"name", 4, &name) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(profile.name)");
	}
	if (generated_expect_string(&region, name, (const uint8_t *)"Ada", 3,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6, &scores) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(scores)");
	}
	if (!jsval_is_json_backed(scores)) {
		return generated_failf(detail, cap, "expected parsed scores to stay JSON-backed");
	}
	if (jsval_array_get(&region, scores, 1, &second_score) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(scores[1])");
	}
	if (generated_expect_number(&region, second_score, 2.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}

	errno = 0;
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"note", 4,
			jsval_null()) == 0) {
		return generated_failf(detail, cap,
				"JSON-backed object mutation unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap, "expected ENOTSUP before promotion, got %d",
				errno);
	}

	errno = 0;
	if (jsval_array_set(&region, scores, 0, jsval_number(7.0)) == 0) {
		return generated_failf(detail, cap, "JSON-backed array mutation unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap, "expected ENOTSUP before array promotion, got %d",
				errno);
	}

	if (jsval_region_promote_root(&region, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_promote_root");
	}
	if (!jsval_is_native(root)) {
		return generated_failf(detail, cap, "expected promoted root to be native");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7, &profile) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(promoted profile)");
	}
	if (!jsval_is_native(profile)) {
		return generated_failf(detail, cap, "expected promoted child object to be native");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6, &scores) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(promoted scores)");
	}
	if (!jsval_is_native(scores)) {
		return generated_failf(detail, cap, "expected promoted child array to be native");
	}
	if (jsval_object_get_utf8(&region, profile, (const uint8_t *)"active", 6, &active) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(profile.active)");
	}
	if (active.kind != JSVAL_KIND_BOOL || active.as.boolean != 1) {
		return generated_failf(detail, cap, "expected profile.active to start true");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"updated", 7, &replacement) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(updated)");
	}
	if (jsval_object_set_utf8(&region, profile, (const uint8_t *)"active", 6,
			jsval_bool(0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(profile.active)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"note", 4, replacement) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(note)");
	}
	if (jsval_array_set(&region, scores, 0, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(scores[0])");
	}

	return generated_expect_json(&region, root, expected, sizeof(expected) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_values(char *detail, size_t cap)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t empty;
	jsval_t one_string;
	jsval_t x_string;
	jsval_t same_a;
	jsval_t same_b;
	jsval_t sum;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(empty)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(one)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &x_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(x)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(same_a)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"same", 4, &same_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(same_b)");
	}

	if (jsval_truthy(&region, jsval_number(1.0)) != 1
			|| jsval_truthy(&region, jsval_bool(1)) != 1
			|| jsval_truthy(&region, one_string) != 1) {
		return generated_failf(detail, cap, "expected primitive truthy cases to stay truthy");
	}
	if (jsval_truthy(&region, jsval_number(0.0)) != 0
			|| jsval_truthy(&region, jsval_bool(0)) != 0
			|| jsval_truthy(&region, jsval_null()) != 0
			|| jsval_truthy(&region, jsval_undefined()) != 0
			|| jsval_truthy(&region, empty) != 0) {
		return generated_failf(detail, cap, "expected primitive falsy cases to stay falsy");
	}
	if (jsval_strict_eq(&region, same_a, same_b) != 1
			|| jsval_strict_eq(&region, jsval_number(+0.0), jsval_number(-0.0)) != 1
			|| jsval_strict_eq(&region, jsval_number(1.0), one_string) != 0
			|| jsval_strict_eq(&region, jsval_bool(1), jsval_number(1.0)) != 0
			|| jsval_strict_eq(&region, jsval_null(), jsval_undefined()) != 0) {
		return generated_failf(detail, cap, "unexpected strict equality result");
	}

	if (jsval_add(&region, jsval_number(1.0), jsval_number(1.0), &sum) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(number, number)");
	}
	status = generated_expect_number(&region, sum, 2.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_add(&region, one_string, jsval_number(1.0), &sum) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(string, number)");
	}
	status = generated_expect_string(&region, sum, (const uint8_t *)"11", 2, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_add(&region, jsval_number(1.0), x_string, &sum) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(number, string)");
	}
	return generated_expect_string(&region, sum, (const uint8_t *)"1x", 2,
			detail, cap);
}

static generated_status_t generated_smoke_jsval_typeof(char *detail, size_t cap)
{
	static const uint8_t json[] =
		"{\"flag\":true,\"num\":1,\"text\":\"x\",\"nothing\":null,\"obj\":{},\"arr\":[]}";
	uint8_t storage[8192];
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
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_typeof(&region, jsval_undefined(), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(undefined)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"undefined", 9, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, jsval_null(), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(null)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, jsval_bool(1), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(bool)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"boolean", 7, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(number)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"number",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&native_text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(x)");
	}
	if (jsval_object_new(&region, 0, &native_obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new");
	}
	if (jsval_array_new(&region, 0, &native_arr) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new");
	}
	if (jsval_typeof(&region, native_text, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(native string)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"string",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, native_obj, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(native object)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, native_arr, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(native array)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_json_parse(&region, json, sizeof(json) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(flag)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(num)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(nothing)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(obj)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(arr)");
	}

	if (jsval_typeof(&region, flag, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(parsed bool)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"boolean", 7, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, num, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(parsed number)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"number",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, text, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(parsed string)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"string",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, nothing, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(parsed null)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, obj, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(parsed object)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_typeof(&region, arr, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(parsed array)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

#if JSMX_WITH_REGEX
	{
		jsmethod_error_t error;
		jsval_t pattern;
		jsval_t global_flags;
		jsval_t regex;
		jsval_t subject;
		jsval_t iterator;

		if (jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&pattern) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_string_new_utf8(regex pattern)");
		}
		if (jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_string_new_utf8(regex flags)");
		}
		if (jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
				&subject) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_string_new_utf8(regex subject)");
		}
		if (jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
				&error) < 0) {
			return generated_failf(detail, cap,
					"jsval_regexp_new failed: errno=%d kind=%d", errno,
					error.kind);
		}
		if (jsval_typeof(&region, regex, &result) < 0) {
			return generated_fail_errno(detail, cap, "jsval_typeof(regex)");
		}
		status = generated_expect_string(&region, result,
				(const uint8_t *)"object", 6, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_method_string_match_all(&region, subject, 1, regex, &iterator,
				&error) < 0) {
			return generated_failf(detail, cap,
					"jsval_method_string_match_all failed: errno=%d kind=%d",
					errno, error.kind);
		}
		if (jsval_typeof(&region, iterator, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_typeof(matchAll iterator)");
		}
		status = generated_expect_string(&region, result,
				(const uint8_t *)"object", 6, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
	}
#endif

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_set(char *detail, size_t cap)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t set;
	jsval_t grown;
	jsval_t string_a;
	jsval_t string_b;
	jsval_t object_a;
	jsval_t object_b;
	jsval_t result;
	size_t size = 0;
	int has = 0;
	int deleted = 0;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_set_new(&region, 2, &set) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_new");
	}
	if (!jsval_truthy(&region, set)) {
		return generated_failf(detail, cap, "expected set to be truthy");
	}
	if (jsval_typeof(&region, set, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(set)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_strict_eq(&region, set, set) != 1) {
		return generated_failf(detail, cap,
				"expected set identity to be strict-equal to itself");
	}

	if (jsval_set_add(&region, set, jsval_number(NAN)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(NaN)");
	}
	if (jsval_set_has(&region, set, jsval_number(NAN), &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(NaN)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_set_add(&region, set, jsval_number(-0.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(-0)");
	}
	if (jsval_set_has(&region, set, jsval_number(+0.0), &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(+0)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_set_size(&region, set, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_size(initial)");
	}
	if (size != 2) {
		return generated_failf(detail, cap,
				"expected initial set size 2, got %zu", size);
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(dup a)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(dup b)");
	}
	errno = 0;
	if (jsval_set_add(&region, set, string_a) != -1 || errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected set add overflow to fail with ENOBUFS");
	}
	if (jsval_set_clone(&region, set, 4, &grown) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_clone");
	}
	if (jsval_strict_eq(&region, set, grown) != 0) {
		return generated_failf(detail, cap,
				"expected cloned set to have distinct identity");
	}
	set = grown;

	if (jsval_set_add(&region, set, string_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(string_a)");
	}
	if (jsval_set_has(&region, set, string_b, &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(string_b)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_set_add(&region, set, string_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(string_b)");
	}
	if (jsval_set_size(&region, set, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_size(strings)");
	}
	if (size != 3) {
		return generated_failf(detail, cap,
				"expected string-deduped set size 3, got %zu", size);
	}

	if (jsval_object_new(&region, 0, &object_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(object_a)");
	}
	if (jsval_object_new(&region, 0, &object_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(object_b)");
	}
	if (jsval_set_add(&region, set, object_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(object_a)");
	}
	if (jsval_set_has(&region, set, object_a, &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(object_a)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_set_has(&region, set, object_b, &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(object_b)");
	}
	status = generated_expect_boolean_result(has, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_set_delete(&region, set, jsval_number(+0.0), &deleted) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_delete(+0)");
	}
	status = generated_expect_boolean_result(deleted, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_set_has(&region, set, jsval_number(-0.0), &has) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_set_has(-0 after delete)");
	}
	status = generated_expect_boolean_result(has, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_set_clear(&region, set) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_clear");
	}
	if (jsval_set_size(&region, set, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_size(clear)");
	}
	if (size != 0) {
		return generated_failf(detail, cap,
				"expected cleared set size 0, got %zu", size);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_map(char *detail, size_t cap)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t map;
	jsval_t grown;
	jsval_t string_a;
	jsval_t string_b;
	jsval_t object_a;
	jsval_t object_b;
	jsval_t result;
	size_t size = 0;
	int has = 0;
	int deleted = 0;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_map_new(&region, 2, &map) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_new");
	}
	if (!jsval_truthy(&region, map)) {
		return generated_failf(detail, cap, "expected map to be truthy");
	}
	if (jsval_typeof(&region, map, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(map)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"object",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_strict_eq(&region, map, map) != 1) {
		return generated_failf(detail, cap,
				"expected map identity to be strict-equal to itself");
	}
	if (jsval_map_get(&region, map, jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(missing)");
	}
	if (result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected missing map get to return undefined");
	}

	if (jsval_map_set(&region, map, jsval_number(NAN), jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(NaN)");
	}
	if (jsval_map_has(&region, map, jsval_number(NAN), &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_has(NaN)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_get(&region, map, jsval_number(NAN), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(NaN)");
	}
	status = generated_expect_number(&region, result, 1.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_map_set(&region, map, jsval_number(-0.0), jsval_number(2.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(-0)");
	}
	if (jsval_map_has(&region, map, jsval_number(+0.0), &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_has(+0)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_size(&region, map, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_size(initial)");
	}
	if (size != 2) {
		return generated_failf(detail, cap,
				"expected initial map size 2, got %zu", size);
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(dup a)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"dup", 3,
			&string_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(dup b)");
	}
	errno = 0;
	if (jsval_map_set(&region, map, string_a, jsval_number(3.0)) != -1
			|| errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected map set overflow to fail with ENOBUFS");
	}
	if (jsval_map_clone(&region, map, 4, &grown) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_clone");
	}
	if (jsval_strict_eq(&region, map, grown) != 0) {
		return generated_failf(detail, cap,
				"expected cloned map to have distinct identity");
	}
	map = grown;

	if (jsval_map_set(&region, map, string_a, jsval_number(3.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(string_a)");
	}
	if (jsval_map_has(&region, map, string_b, &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_has(string_b)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_get(&region, map, string_b, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(string_b)");
	}
	status = generated_expect_number(&region, result, 3.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_set(&region, map, string_b, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(string_b)");
	}
	if (jsval_map_size(&region, map, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_size(strings)");
	}
	if (size != 3) {
		return generated_failf(detail, cap,
				"expected deduped map size 3, got %zu", size);
	}
	if (jsval_map_key_at(&region, map, 2, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_key_at(2)");
	}
	if (jsval_strict_eq(&region, result, string_a) != 1) {
		return generated_failf(detail, cap,
				"expected overwrite to preserve original map key order");
	}
	if (jsval_map_value_at(&region, map, 2, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_value_at(2)");
	}
	status = generated_expect_number(&region, result, 4.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_object_new(&region, 0, &object_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(object_a)");
	}
	if (jsval_object_new(&region, 0, &object_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(object_b)");
	}
	if (jsval_map_set(&region, map, object_a, jsval_number(5.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(object_a)");
	}
	if (jsval_map_has(&region, map, object_a, &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_has(object_a)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_has(&region, map, object_b, &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_has(object_b)");
	}
	status = generated_expect_boolean_result(has, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_map_delete(&region, map, string_b, &deleted) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_delete(string_b)");
	}
	status = generated_expect_boolean_result(deleted, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_key_at(&region, map, 2, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_key_at(compacted)");
	}
	if (jsval_strict_eq(&region, result, object_a) != 1) {
		return generated_failf(detail, cap,
				"expected map delete compaction to preserve remaining order");
	}
	if (jsval_map_delete(&region, map, jsval_number(+0.0), &deleted) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_delete(+0)");
	}
	status = generated_expect_boolean_result(deleted, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_map_get(&region, map, jsval_number(-0.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(-0 after delete)");
	}
	if (result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected deleted numeric key to read back as undefined");
	}

	if (jsval_map_clear(&region, map) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_clear");
	}
	if (jsval_map_size(&region, map, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_size(clear)");
	}
	if (size != 0) {
		return generated_failf(detail, cap,
				"expected cleared map size 0, got %zu", size);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_symbol(char *detail,
		size_t cap)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t description;
	jsval_t symbol_a;
	jsval_t symbol_b;
	jsval_t symbol_none;
	jsval_t object;
	jsval_t clone;
	jsval_t copied;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int boolean = 0;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_string_new_utf8(&region, (const uint8_t *)"token", 5,
			&description) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(symbol description)");
	}
	if (jsval_symbol_new(&region, 1, description, &symbol_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_symbol_new(symbol_a)");
	}
	if (jsval_symbol_new(&region, 1, description, &symbol_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_symbol_new(symbol_b)");
	}
	if (jsval_symbol_new(&region, 0, jsval_undefined(), &symbol_none) < 0) {
		return generated_fail_errno(detail, cap, "jsval_symbol_new(symbol_none)");
	}
	if (!jsval_truthy(&region, symbol_a)) {
		return generated_failf(detail, cap, "expected symbol to be truthy");
	}
	if (jsval_typeof(&region, symbol_a, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(symbol)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"symbol",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_strict_eq(&region, symbol_a, symbol_a) != 1) {
		return generated_failf(detail, cap,
				"expected symbol identity to be strict-equal to itself");
	}
	if (jsval_strict_eq(&region, symbol_a, symbol_b) != 0) {
		return generated_failf(detail, cap,
				"expected distinct symbols with the same description to differ");
	}
	if (jsval_abstract_eq(&region, symbol_a, symbol_b, &boolean) < 0) {
		return generated_fail_errno(detail, cap, "jsval_abstract_eq(symbols)");
	}
	status = generated_expect_boolean_result(boolean, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_symbol_description(&region, symbol_a, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_symbol_description(a)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"token",
			5, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_symbol_description(&region, symbol_none, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_symbol_description(undefined)");
	}
	if (result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected symbol without description to report undefined");
	}

	memset(&error, 0, sizeof(error));
	if (jsval_method_string_index_of(&region, symbol_a, description, 0,
			jsval_undefined(), &result, &error) != -1
			|| error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_failf(detail, cap,
				"expected symbol string coercion to fail with a type error");
	}

	if (jsval_object_new(&region, 3, &object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(symbol object)");
	}
	if (jsval_object_set_key(&region, object, symbol_a, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_key(symbol_a)");
	}
	if (jsval_object_set_utf8(&region, object, (const uint8_t *)"plain", 5,
			jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(plain)");
	}
	if (jsval_object_set_key(&region, object, symbol_b, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_key(symbol_b)");
	}
	if (jsval_object_has_own_key(&region, object, symbol_a, &boolean) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_has_own_key(symbol_a)");
	}
	status = generated_expect_boolean_result(boolean, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_key(&region, object, symbol_a, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_key(symbol_a)");
	}
	status = generated_expect_number(&region, value, 7.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_key_at(&region, object, 0, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_key_at(0)");
	}
	if (jsval_strict_eq(&region, value, symbol_a) != 1) {
		return generated_failf(detail, cap,
				"expected first own key to be the first symbol");
	}
	if (jsval_object_key_at(&region, object, 1, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_key_at(1)");
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"plain",
			5, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_key_at(&region, object, 2, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_key_at(2)");
	}
	if (jsval_strict_eq(&region, value, symbol_b) != 1) {
		return generated_failf(detail, cap,
				"expected third own key to be the second symbol");
	}
	status = generated_expect_json(&region, object,
			(const uint8_t *)"{\"plain\":true}", 14, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_object_clone_own(&region, object, 3, &clone) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_clone_own(symbol)");
	}
	if (jsval_object_get_key(&region, clone, symbol_b, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_key(clone symbol_b)");
	}
	status = generated_expect_number(&region, value, 9.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_object_new(&region, 3, &copied) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(copied)");
	}
	if (jsval_object_copy_own(&region, copied, object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_copy_own(symbol)");
	}
	if (jsval_object_get_key(&region, copied, symbol_a, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_key(copied symbol_a)");
	}
	status = generated_expect_number(&region, value, 7.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_object_delete_key(&region, object, symbol_a, &boolean) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_delete_key(symbol_a)");
	}
	status = generated_expect_boolean_result(boolean, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_has_own_key(&region, object, symbol_a, &boolean) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_has_own_key(after delete)");
	}
	status = generated_expect_boolean_result(boolean, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_bigint(char *detail,
		size_t cap)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t zero;
	jsval_t forty_two;
	jsval_t forty_two_text;
	jsval_t decimal_text;
	jsval_t left_big;
	jsval_t right_big;
	jsval_t value;
	jsval_t set;
	jsval_t map;
	int boolean = 0;
	int cmp = 0;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_bigint_new_i64(&region, 0, &zero) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bigint_new_i64(zero)");
	}
	if (jsval_truthy(&region, zero)) {
		return generated_failf(detail, cap, "expected 0n to be falsy");
	}
	if (jsval_typeof(&region, zero, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(bigint)");
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"bigint",
			6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_bigint_new_i64(&region, 42, &forty_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_bigint_new_i64(forty_two)");
	}
	if (jsval_bigint_new_utf8(&region, (const uint8_t *)"42", 2,
			&forty_two_text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_bigint_new_utf8(forty_two_text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"42", 2,
			&decimal_text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(42)");
	}
	if (jsval_strict_eq(&region, forty_two, forty_two_text) != 1) {
		return generated_failf(detail, cap,
				"expected equal bigint values to be strict-equal");
	}
	if (jsval_abstract_eq(&region, forty_two, decimal_text, &boolean) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_eq(bigint,string)");
	}
	status = generated_expect_boolean_result(boolean, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_bigint_new_utf8(&region, (const uint8_t *)"12345678901234567890",
			20, &left_big) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_bigint_new_utf8(left_big)");
	}
	if (jsval_bigint_new_utf8(&region, (const uint8_t *)"9876543210", 10,
			&right_big) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_bigint_new_utf8(right_big)");
	}
	if (jsval_add(&region, left_big, right_big, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(bigint)");
	}
	status = generated_expect_bigint(&region, value,
			(const uint8_t *)"12345678911111111100", 20, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_subtract(&region, left_big, right_big, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_subtract(bigint)");
	}
	status = generated_expect_bigint(&region, value,
			(const uint8_t *)"12345678891358024680", 20, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_multiply(&region, left_big, right_big, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_multiply(bigint)");
	}
	status = generated_expect_bigint(&region, value,
			(const uint8_t *)"121932631124828532111263526900", 30,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_unary_minus(&region, forty_two, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_unary_minus(bigint)");
	}
	status = generated_expect_bigint(&region, value, (const uint8_t *)"-42", 3,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_bigint_compare(&region, left_big, right_big, &cmp) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bigint_compare");
	}
	if (cmp <= 0) {
		return generated_failf(detail, cap,
				"expected left_big to compare greater than right_big");
	}

	errno = 0;
	if (jsval_add(&region, forty_two, jsval_number(1.0), &value) != -1
			|| errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected mixed Number/BigInt add to fail with ENOTSUP");
	}
	errno = 0;
	if (jsval_unary_plus(&region, forty_two, &value) != -1
			|| errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected unary plus on bigint to fail with ENOTSUP");
	}

	if (jsval_set_new(&region, 1, &set) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_new(bigint)");
	}
	if (jsval_set_add(&region, set, forty_two) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(bigint)");
	}
	if (jsval_set_has(&region, set, forty_two_text, &boolean) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(bigint)");
	}
	status = generated_expect_boolean_result(boolean, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_map_new(&region, 1, &map) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_new(bigint)");
	}
	if (jsval_map_set(&region, map, forty_two, jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(bigint)");
	}
	if (jsval_map_get(&region, map, forty_two_text, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(bigint)");
	}
	if (value.kind != JSVAL_KIND_BOOL || value.as.boolean != 1) {
		return generated_failf(detail, cap,
				"expected bigint-keyed map lookup to return true");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_function(char *detail,
		size_t cap)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t name;
	jsval_t named;
	jsval_t anonymous;
	jsval_t named_again;
	jsval_t echoer;
	jsval_t thrower;
	jsval_t object;
	jsval_t array;
	jsval_t set;
	jsval_t map;
	jsval_t value;
	jsval_t result;
	jsval_t args[2];
	int boolean = 0;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_string_new_utf8(&region, (const uint8_t *)"sum", 3, &name) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(sum)");
	}
	if (jsval_function_new(&region, generated_static_sum_function, 2, 1, name,
			&named) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_new(named)");
	}
	if (jsval_function_new(&region, generated_static_sum_function, 2, 0,
			jsval_undefined(), &anonymous) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_function_new(anonymous)");
	}
	if (jsval_function_new(&region, generated_static_sum_function, 2, 1, name,
			&named_again) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_function_new(named_again)");
	}
	if (jsval_function_new(&region, generated_static_echo_function, 1, 0,
			jsval_undefined(), &echoer) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_new(echoer)");
	}
	if (jsval_function_new(&region, generated_static_throw_function, 0, 1, name,
			&thrower) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_new(thrower)");
	}

	if (!jsval_truthy(&region, named)) {
		return generated_failf(detail, cap, "expected function value to be truthy");
	}
	if (jsval_typeof(&region, named, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(function)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"function", 8, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_strict_eq(&region, named, named) != 1) {
		return generated_failf(detail, cap,
				"expected reused function value to preserve identity");
	}
	if (jsval_strict_eq(&region, named, named_again) != 0) {
		return generated_failf(detail, cap,
				"expected distinct function allocations to stay distinct");
	}
	if (jsval_abstract_eq(&region, named, named_again, &boolean) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_eq(function,function)");
	}
	status = generated_expect_boolean_result(boolean, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_function_name(&region, named, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_name(named)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"sum", 3,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_function_name(&region, anonymous, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_function_name(anonymous)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"", 0,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_function_length(&region, named, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_length(named)");
	}
	status = generated_expect_number(&region, result, 2.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	args[0] = jsval_number(20.0);
	args[1] = jsval_number(22.0);
	memset(&error, 0, sizeof(error));
	if (jsval_function_call(&region, named, 2, args, &result, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_call(named)");
	}
	status = generated_expect_number(&region, result, 42.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_object_new(&region, 1, &object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(function)");
	}
	if (jsval_object_set_utf8(&region, object, (const uint8_t *)"fn", 2, named)
			< 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(function)");
	}
	if (jsval_object_get_utf8(&region, object, (const uint8_t *)"fn", 2,
			&value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(function)");
	}
	if (jsval_strict_eq(&region, value, named) != 1) {
		return generated_failf(detail, cap,
				"expected function property readback to preserve identity");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_function_call(&region, value, 2, args, &result, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_function_call(object property)");
	}
	status = generated_expect_number(&region, result, 42.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_array_new(&region, 1, &array) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(function)");
	}
	if (jsval_array_push(&region, array, echoer) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(function)");
	}
	if (jsval_array_get(&region, array, 0, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(function)");
	}
	if (jsval_strict_eq(&region, value, echoer) != 1) {
		return generated_failf(detail, cap,
				"expected array-stored function to preserve identity");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"echo", 4, &args[0])
			< 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(echo)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_function_call(&region, value, 1, args, &result, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_function_call(array function)");
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"echo",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_set_new(&region, 1, &set) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_new(function)");
	}
	if (jsval_set_add(&region, set, named) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_add(function)");
	}
	if (jsval_set_has(&region, set, named, &boolean) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(function)");
	}
	status = generated_expect_boolean_result(boolean, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_set_has(&region, set, named_again, &boolean) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_set_has(distinct function)");
	}
	status = generated_expect_boolean_result(boolean, 0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_map_new(&region, 1, &map) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_new(function)");
	}
	if (jsval_map_set(&region, map, named, jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_set(function)");
	}
	if (jsval_map_get(&region, map, named, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(function)");
	}
	if (value.kind != JSVAL_KIND_BOOL || value.as.boolean != 1) {
		return generated_failf(detail, cap,
				"expected function-keyed map lookup to return true");
	}

	memset(&error, 0, sizeof(error));
	errno = 0;
	if (jsval_function_call(&region, thrower, 0, NULL, &result, &error) != -1) {
		return generated_failf(detail, cap,
				"expected generated throw function to fail");
	}
	if (errno != EINVAL || error.kind != JSMETHOD_ERROR_ABRUPT
			|| error.message == NULL
			|| strcmp(error.message, "generated function threw") != 0) {
		return generated_failf(detail, cap,
				"expected abrupt function call error propagation");
	}

	memset(&error, 0, sizeof(error));
	errno = 0;
	if (jsval_function_call(&region, jsval_number(1.0), 0, NULL, &result,
			&error) != -1) {
		return generated_failf(detail, cap,
				"expected non-function call to fail");
	}
	if (errno != EINVAL || error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_failf(detail, cap,
				"expected non-function call to report a type error");
	}

	errno = 0;
	if (jsval_copy_json(&region, named, NULL, 0, NULL) != -1
			|| errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected JSON emission of a function value to fail");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_iterators(char *detail,
		size_t cap)
{
	static const char json_string[] = "\"a\\ud834\\udf06b\"";
	static const uint16_t astral_pair_units[] = {0xD834, 0xDF06, 'b'};
	static const uint16_t lone_high_units[] = {0xD834};
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t array;
	jsval_t iterator;
	jsval_t value;
	jsval_t key;
	jsval_t set;
	jsval_t pairs;
	jsval_t pair;
	jsval_t map;
	jsval_t string_a;
	jsval_t string_b;
	jsval_t string_value;
	jsval_t parsed_string;
	size_t size = 0;
	int done = 0;
	int has = 0;
	double sum = 0.0;
	generated_status_t status;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_array_new(&region, 4, &array) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(array)");
	}
	if (jsval_array_push(&region, array, jsval_number(1.0)) < 0
			|| jsval_array_push(&region, array, jsval_number(2.0)) < 0
			|| jsval_array_push(&region, array, jsval_number(3.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(array)");
	}
	if (jsval_get_iterator(&region, array, JSVAL_ITERATOR_SELECTOR_DEFAULT,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(array values) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (!jsval_truthy(&region, iterator)) {
		return generated_failf(detail, cap, "expected iterator to be truthy");
	}
	if (jsval_typeof(&region, iterator, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(iterator)");
	}
	status = generated_expect_string(&region, value,
			(const uint8_t *)"object", 6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	for (;;) {
		if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
			return generated_failf(detail, cap,
					"jsval_iterator_next(array values) failed: errno=%d kind=%d",
					errno, error.kind);
		}
		if (done) {
			break;
		}
		if (value.kind != JSVAL_KIND_NUMBER) {
			return generated_failf(detail, cap,
					"expected numeric array iterator value");
		}
		sum += value.as.number;
	}
	if (sum != 6.0) {
		return generated_failf(detail, cap,
				"expected array iterator sum 6, got %.17g", sum);
	}

	if (jsval_get_iterator(&region, array, JSVAL_ITERATOR_SELECTOR_ENTRIES,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(array entries) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	errno = 0;
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) != -1
			|| errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP from jsval_iterator_next(array entries)");
	}
	if (jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next_entry(array entries) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (done || key.kind != JSVAL_KIND_NUMBER || key.as.number != 0.0
			|| value.kind != JSVAL_KIND_NUMBER || value.as.number != 1.0) {
		return generated_failf(detail, cap,
				"unexpected first array entry iterator result");
	}

	if (jsval_set_new(&region, 2, &set) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_new(iterable)");
	}
	if (jsval_get_iterator(&region, array, JSVAL_ITERATOR_SELECTOR_VALUES,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(array for set) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	for (;;) {
		if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
			return generated_failf(detail, cap,
					"jsval_iterator_next(array for set) failed: errno=%d kind=%d",
					errno, error.kind);
		}
		if (done) {
			break;
		}
		if (value.kind == JSVAL_KIND_NUMBER && value.as.number == 3.0) {
			value = jsval_number(2.0);
		}
		if (jsval_set_add(&region, set, value) < 0) {
			return generated_fail_errno(detail, cap, "jsval_set_add(iterable)");
		}
	}
	if (jsval_set_size(&region, set, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_size(iterable)");
	}
	if (size != 2) {
		return generated_failf(detail, cap,
				"expected Set(iterable) size 2, got %zu", size);
	}
	if (jsval_set_has(&region, set, jsval_number(2.0), &has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_has(iterable)");
	}
	status = generated_expect_boolean_result(has, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"a", 1,
			&string_a) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(a)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"b", 1,
			&string_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(b)");
	}
	if (jsval_array_new(&region, 2, &pairs) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(pairs)");
	}
	if (jsval_array_new(&region, 2, &pair) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(pair a)");
	}
	if (jsval_array_push(&region, pair, string_a) < 0
			|| jsval_array_push(&region, pair, jsval_number(5.0)) < 0
			|| jsval_array_push(&region, pairs, pair) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(pair a)");
	}
	if (jsval_array_new(&region, 2, &pair) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(pair b)");
	}
	if (jsval_array_push(&region, pair, string_b) < 0
			|| jsval_array_push(&region, pair, jsval_number(6.0)) < 0
			|| jsval_array_push(&region, pairs, pair) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(pair b)");
	}
	if (jsval_map_new(&region, 2, &map) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_new(iterable)");
	}
	if (jsval_get_iterator(&region, pairs, JSVAL_ITERATOR_SELECTOR_VALUES,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(pairs) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	for (;;) {
		jsval_t pair_key;
		jsval_t pair_value;

		if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
			return generated_failf(detail, cap,
					"jsval_iterator_next(pairs) failed: errno=%d kind=%d",
					errno, error.kind);
		}
		if (done) {
			break;
		}
		if (jsval_array_get(&region, value, 0, &pair_key) < 0
				|| jsval_array_get(&region, value, 1, &pair_value) < 0) {
			return generated_fail_errno(detail, cap, "jsval_array_get(pair)");
		}
		if (jsval_map_set(&region, map, pair_key, pair_value) < 0) {
			return generated_fail_errno(detail, cap, "jsval_map_set(iterable)");
		}
	}
	if (jsval_map_get(&region, map, string_a, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_map_get(a)");
	}
	status = generated_expect_number(&region, value, 5.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_get_iterator(&region, map, JSVAL_ITERATOR_SELECTOR_DEFAULT,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(map entries) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	errno = 0;
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) != -1
			|| errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP from jsval_iterator_next(map entries)");
	}
	if (jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next_entry(map entries) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (done || jsval_strict_eq(&region, key, string_a) != 1
			|| value.kind != JSVAL_KIND_NUMBER || value.as.number != 5.0) {
		return generated_failf(detail, cap,
				"unexpected first map entry iterator result");
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"abc", 3,
			&string_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(abc)");
	}
	if (jsval_get_iterator(&region, string_value,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(string values) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(string first) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"a", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(string second) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"b", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(string third) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"c", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(string done) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (!done || value.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected string iterator exhaustion");
	}

	if (jsval_string_new_utf16(&region, astral_pair_units,
			sizeof(astral_pair_units) / sizeof(astral_pair_units[0]),
			&string_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf16(astral)");
	}
	if (jsval_get_iterator(&region, string_value,
			JSVAL_ITERATOR_SELECTOR_ENTRIES, &iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(string entries) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	errno = 0;
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) != -1
			|| errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP from jsval_iterator_next(string entries)");
	}
	if (jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next_entry(string first) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (done || key.kind != JSVAL_KIND_NUMBER || key.as.number != 0.0) {
		return generated_failf(detail, cap,
				"unexpected first string entry key");
	}
	status = generated_expect_utf16_string(&region, value, astral_pair_units, 2,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_iterator_next_entry(&region, iterator, &done, &key, &value,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next_entry(string second) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (done || key.kind != JSVAL_KIND_NUMBER || key.as.number != 2.0) {
		return generated_failf(detail, cap,
				"unexpected second string entry key");
	}
	status = generated_expect_utf16_string(&region, value,
			astral_pair_units + 2, 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf16(&region, lone_high_units,
			sizeof(lone_high_units) / sizeof(lone_high_units[0]),
			&string_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16(lone surrogate)");
	}
	if (jsval_get_iterator(&region, string_value,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(lone surrogate) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(lone surrogate) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_utf16_string(&region, value, lone_high_units, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_json_parse(&region, (const uint8_t *)json_string,
			sizeof(json_string) - 1, 16, &parsed_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(string)");
	}
	if (jsval_get_iterator(&region, parsed_string,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(parsed string) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(parsed string first) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"a", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(parsed string second) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_utf16_string(&region, value, astral_pair_units, 2,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_iterator_next(parsed string third) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"b", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_set_new(&region, 3, &set) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_new(string iterable)");
	}
	if (jsval_get_iterator(&region, parsed_string,
			JSVAL_ITERATOR_SELECTOR_DEFAULT, &iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_get_iterator(parsed string for set) failed: errno=%d kind=%d",
				errno, error.kind);
	}
	for (;;) {
		if (jsval_iterator_next(&region, iterator, &done, &value, &error) < 0) {
			return generated_failf(detail, cap,
					"jsval_iterator_next(parsed string for set) failed: errno=%d kind=%d",
					errno, error.kind);
		}
		if (done) {
			break;
		}
		if (jsval_set_add(&region, set, value) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_set_add(string iterable)");
		}
	}
	if (jsval_set_size(&region, set, &size) < 0) {
		return generated_fail_errno(detail, cap, "jsval_set_size(string iterable)");
	}
	if (size != 3) {
		return generated_failf(detail, cap,
				"expected Set(string iterable) size 3, got %zu", size);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_date(char *detail,
		size_t cap)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t now_value;
	jsval_t date_value;
	jsval_t parsed_value;
	jsval_t local_value;
	jsval_t invalid_value;
	jsval_t input;
	jsval_t bad_input;
	jsval_t result;
	jsval_t args[7];
	size_t len = 0;
	int valid = 0;
	jsmethod_error_t error;
	generated_status_t status;
	uint8_t buf[128];

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_date_now(&region, &now_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_now");
	}
	if (now_value.kind != JSVAL_KIND_NUMBER || !isfinite(now_value.as.number)) {
		return generated_failf(detail, cap,
				"expected Date.now() helper to return a finite number");
	}
	if (jsval_date_new_now(&region, &date_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_new_now");
	}
	if (!jsval_truthy(&region, date_value)) {
		return generated_failf(detail, cap, "expected Date object to be truthy");
	}
	if (jsval_typeof(&region, date_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(date)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"object", 6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_is_valid(&region, date_value, &valid) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_is_valid(now)");
	}
	if (!valid) {
		return generated_failf(detail, cap, "expected new Date() helper to be valid");
	}

	if (jsval_date_new_time(&region, jsval_number(1577934245006.0),
			&date_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_new_time");
	}
	if (jsval_date_get_utc_full_year(&region, date_value, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_date_get_utc_full_year");
	}
	status = generated_expect_number(&region, result, 2020.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_get_utc_day(&region, date_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_get_utc_day");
	}
	status = generated_expect_number(&region, result, 4.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	memset(&error, 0, sizeof(error));
	if (jsval_date_to_iso_string(&region, date_value, &result, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_to_iso_string");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_to_utc_string(&region, date_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_to_utc_string");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"Thu, 02 Jan 2020 03:04:05 GMT",
			sizeof("Thu, 02 Jan 2020 03:04:05 GMT") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, &input) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(date)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_date_parse_iso(&region, input, &result, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_parse_iso");
	}
	status = generated_expect_number(&region, result, 1577934245006.0, detail,
			cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_new_iso(&region, input, &parsed_value, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_new_iso");
	}
	if (jsval_date_to_iso_string(&region, parsed_value, &result, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_date_to_iso_string(parsed)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"2020-01-02T03:04:05.006Z",
			sizeof("2020-01-02T03:04:05.006Z") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	args[0] = jsval_number(99.0);
	args[1] = jsval_number(0.0);
	args[2] = jsval_number(2.0);
	args[3] = jsval_number(3.0);
	args[4] = jsval_number(4.0);
	args[5] = jsval_number(5.0);
	args[6] = jsval_number(6.0);
	if (jsval_date_utc(&region, 7, args, &result, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_utc");
	}
	status = generated_expect_number(&region, result, 915246245006.0, detail,
			cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_new_local_fields(&region, 7, args, &local_value, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_new_local_fields");
	}
	if (jsval_date_get_full_year(&region, local_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_get_full_year");
	}
	status = generated_expect_number(&region, result, 1999.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_set_hours(&region, local_value, jsval_number(23.0),
			&result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_set_hours");
	}
	if (jsval_date_get_hours(&region, local_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_get_hours");
	}
	status = generated_expect_number(&region, result, 23.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_date_to_string(&region, local_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_to_string");
	}
	if (jsval_string_copy_utf8(&region, result, NULL, 0, &len) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_copy_utf8(local length)");
	}
	if (len == 0 || len >= sizeof(buf)) {
		return generated_failf(detail, cap,
				"expected non-empty local date string");
	}
	if (jsval_string_copy_utf8(&region, result, buf, len, NULL) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_copy_utf8(local copy)");
	}
	buf[len] = '\0';
	if (strstr((const char *)buf, "GMT") == NULL) {
		return generated_failf(detail, cap,
				"expected local date string to include GMT offset");
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"bad-date", 8,
			&bad_input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(bad-date)");
	}
	memset(&error, 0, sizeof(error));
	errno = 0;
	if (jsval_date_parse_iso(&region, bad_input, &result, &error) >= 0
			|| errno != EINVAL || error.kind != JSMETHOD_ERROR_SYNTAX) {
		return generated_failf(detail, cap,
				"expected invalid ISO parse to fail with syntax error");
	}

	if (jsval_date_new_time(&region, jsval_number(NAN), &invalid_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_new_time(NaN)");
	}
	if (jsval_date_get_time(&region, invalid_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_get_time(invalid)");
	}
	status = generated_expect_nan_value(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	memset(&error, 0, sizeof(error));
	errno = 0;
	if (jsval_date_to_iso_string(&region, invalid_value, &result, &error) >= 0
			|| errno != EINVAL || error.kind != JSMETHOD_ERROR_RANGE) {
		return generated_failf(detail, cap,
				"expected invalid date toISOString to fail with range error");
	}
	if (jsval_date_to_json(&region, invalid_value, &result, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_date_to_json(invalid)");
	}
	if (result.kind != JSVAL_KIND_NULL) {
		return generated_failf(detail, cap,
				"expected invalid date JSON result to stay null");
	}

	return GENERATED_PASS;
}

static generated_status_t
generated_smoke_jsval_crypto(char *detail, size_t cap)
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
	static const uint8_t zero_key_16[16] = { 0 };
	static const uint8_t zero_key_32[32] = { 0 };
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
	static const uint8_t masked_key_9_bits[] = { 0xff, 0x80 };
	static const char *expected_sign_verify_ops[] = { "sign", "verify" };
	static const char *expected_sign_ops[] = { "sign" };
	static const char *expected_encrypt_decrypt_ops[] = { "encrypt", "decrypt" };
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t crypto_value;
	jsval_t subtle_a;
	jsval_t subtle_b;
	jsval_t typed_u8;
	jsval_t typed_f32;
	jsval_t digest_input;
	jsval_t digest_input_buffer;
	jsval_t digest_algorithm_object;
	jsval_t digest_promise;
	jsval_t digest_dict_promise;
	jsval_t invalid_algorithm_promise;
	jsval_t invalid_data_promise;
	jsval_t result;
	jsval_t algorithm_name;
	jsval_t key_value;
	jsval_t dom_exception;
	jsmethod_error_t error;
	uint8_t uuid_buf[36];
	uint8_t before[16];
	uint8_t after[16];
	size_t len = 0;
	uint32_t usages = 0;
	int extractable = 0;
	jsval_promise_state_t promise_state;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_crypto_new(&region, &crypto_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_crypto_new");
	}
	if (jsval_typeof(&region, crypto_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(crypto)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"object", 6,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_crypto_subtle(&region, crypto_value, &subtle_a) < 0
			|| jsval_crypto_subtle(&region, crypto_value, &subtle_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_crypto_subtle");
	}
	if (jsval_strict_eq(&region, subtle_a, subtle_b) != 1) {
		return generated_failf(detail, cap,
				"expected stable subtle identity");
	}
	if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16, &typed_u8) < 0
			|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_FLOAT32, 4,
				&typed_f32) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typed_array_new");
	}
	if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
			&digest_input) < 0
			|| jsval_typed_array_buffer(&region, digest_input,
				&digest_input_buffer) < 0) {
		return generated_fail_errno(detail, cap, "digest input allocation");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
			&algorithm_name) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(SHA-256)");
	}
#if JSMX_WITH_CRYPTO
	if (jsval_typed_array_copy_bytes(&region, typed_u8, before, sizeof(before),
			&len) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_typed_array_copy_bytes(before)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_crypto_get_random_values(&region, crypto_value, typed_u8,
			&result, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_crypto_get_random_values");
	}
	if (jsval_strict_eq(&region, result, typed_u8) != 1) {
		return generated_failf(detail, cap,
				"expected getRandomValues to return same typed array");
	}
	if (jsval_typed_array_copy_bytes(&region, typed_u8, after, sizeof(after),
			&len) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_typed_array_copy_bytes(after)");
	}
	if (memcmp(before, after, sizeof(after)) == 0) {
		return generated_failf(detail, cap,
				"expected getRandomValues to mutate bytes");
	}
	memset(&error, 0, sizeof(error));
	errno = 0;
	if (jsval_crypto_get_random_values(&region, crypto_value, typed_f32,
			&result, &error) >= 0 || errno != EINVAL
			|| error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_failf(detail, cap,
				"expected float typed array rejection");
	}
	if (jsval_crypto_random_uuid(&region, crypto_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_crypto_random_uuid");
	}
	if (jsval_string_copy_utf8(&region, result, uuid_buf, sizeof(uuid_buf),
			&len) < 0 || len != 36) {
		return generated_fail_errno(detail, cap, "uuid copy");
	}
	if (uuid_buf[8] != '-' || uuid_buf[13] != '-' || uuid_buf[18] != '-'
			|| uuid_buf[23] != '-' || uuid_buf[14] != '4') {
		return generated_failf(detail, cap, "unexpected uuid shape");
	}

	if (jsval_subtle_crypto_digest(&region, subtle_a, algorithm_name,
			digest_input, &digest_promise) < 0) {
		return generated_fail_errno(detail, cap, "jsval_subtle_crypto_digest");
	}
	if (jsval_promise_state(&region, digest_promise, &promise_state) < 0
			|| promise_state != JSVAL_PROMISE_STATE_PENDING
			|| jsval_microtask_pending(&region) != 1) {
		return generated_failf(detail, cap,
				"expected digest promise to stay pending until drain");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(digest)");
	}
	if (jsval_promise_result(&region, digest_promise, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(digest)");
	}
	status = generated_expect_array_buffer_bytes(&region, result,
			sha256_zeros_16, sizeof(sha256_zeros_16), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_strict_eq(&region, result, digest_input_buffer) != 0) {
		return generated_failf(detail, cap,
				"expected digest to allocate a fresh ArrayBuffer");
	}

	if (jsval_object_new(&region, 1, &digest_algorithm_object) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"SHA-1", 5,
				&result) < 0
			|| jsval_object_set_utf8(&region, digest_algorithm_object,
				(const uint8_t *)"name", 4, result) < 0) {
		return generated_fail_errno(detail, cap,
				"digest algorithm object setup");
	}
	if (jsval_subtle_crypto_digest(&region, subtle_a, digest_algorithm_object,
			digest_input_buffer, &digest_dict_promise) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_subtle_crypto_digest(dict)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(digest dict)");
	}
	if (jsval_promise_result(&region, digest_dict_promise, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(digest dict)");
	}
	status = generated_expect_array_buffer_bytes(&region, result,
			sha1_zeros_16, sizeof(sha1_zeros_16), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"SHA-999", 7,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(SHA-999)");
	}
	if (jsval_subtle_crypto_digest(&region, subtle_a, result, digest_input,
			&invalid_algorithm_promise) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_subtle_crypto_digest(unsupported)");
	}
	if (jsval_promise_result(&region, invalid_algorithm_promise, &dom_exception)
			< 0 || dom_exception.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_failf(detail, cap,
				"expected unsupported digest to reject with DOMException");
	}
	if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_dom_exception_name(unsupported digest)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"NotSupportedError", 17, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_subtle_crypto_digest(&region, subtle_a, algorithm_name,
			jsval_number(5.0), &invalid_data_promise) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_subtle_crypto_digest(invalid data)");
	}
	if (jsval_promise_result(&region, invalid_data_promise, &dom_exception) < 0
			|| dom_exception.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_failf(detail, cap,
				"expected invalid digest input to reject with DOMException");
	}
	if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_dom_exception_name(invalid digest data)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"TypeError", 9, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	{
		jsval_t usages_array;
		jsval_t hash_object;
		jsval_t hmac_algorithm;
		jsval_t hmac_name;
		jsval_t raw_format;
		jsval_t jwk_format;
		jsval_t generated_key_promise;
		jsval_t generated_key;
		jsval_t exported_raw_promise;
		jsval_t exported_jwk_promise;
		jsval_t sign_promise;
		jsval_t verify_promise;
		jsval_t verify_false_promise;
		jsval_t raw_import_key_promise;
		jsval_t raw_import_key;
		jsval_t zero_key_input;
		jsval_t sign_usages;
		jsval_t sign_usage;
		jsval_t jwk_object;
		jsval_t jwk_import_key_promise;
		jsval_t jwk_import_key;
		jsval_t jwk_export_raw_promise;
		jsval_t jwk_export_jwk_promise;
		jsval_t empty_usages;
		jsval_t empty_usages_promise;
		jsval_t non_extractable_key_promise;
		jsval_t non_extractable_key;
		jsval_t non_extractable_export_promise;
		jsval_t unsupported_format_promise;
		jsval_t prop;
		jsval_t algorithm_object;

		if (jsval_array_new(&region, 2, &usages_array) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&result) < 0
				|| jsval_array_push(&region, usages_array, result) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
					&result) < 0
				|| jsval_array_push(&region, usages_array, result) < 0
				|| jsval_object_new(&region, 1, &hash_object) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
					&result) < 0
				|| jsval_object_set_utf8(&region, hash_object,
					(const uint8_t *)"name", 4, result) < 0
				|| jsval_object_new(&region, 3, &hmac_algorithm) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
					&hmac_name) < 0
				|| jsval_object_set_utf8(&region, hmac_algorithm,
					(const uint8_t *)"name", 4, hmac_name) < 0
				|| jsval_object_set_utf8(&region, hmac_algorithm,
					(const uint8_t *)"hash", 4, hash_object) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
					&raw_format) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
					&jwk_format) < 0) {
			return generated_fail_errno(detail, cap, "hmac setup");
		}
		if (jsval_subtle_crypto_generate_key(&region, subtle_a, hmac_algorithm, 1,
				usages_array, &generated_key_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_generate_key");
		}
		if (jsval_promise_state(&region, generated_key_promise, &promise_state) < 0
				|| promise_state != JSVAL_PROMISE_STATE_PENDING) {
			return generated_failf(detail, cap,
					"expected generateKey promise to stay pending");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_microtask_drain(generateKey)");
		}
		if (jsval_promise_result(&region, generated_key_promise, &generated_key)
				< 0) {
			return generated_fail_errno(detail, cap,
					"jsval_promise_result(generateKey)");
		}
		if (generated_key.kind != JSVAL_KIND_CRYPTO_KEY) {
			return generated_failf(detail, cap, "expected CryptoKey result");
		}
		if (jsval_crypto_key_algorithm(&region, generated_key, &result) < 0
				|| result.kind != JSVAL_KIND_OBJECT) {
			return generated_fail_errno(detail, cap,
					"jsval_crypto_key_algorithm(generated)");
		}
		algorithm_object = result;
		if (jsval_object_get_utf8(&region, algorithm_object,
				(const uint8_t *)"name", 4,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "algorithm.name");
		}
		status = generated_expect_string(&region, prop, (const uint8_t *)"HMAC",
				4, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_object_get_utf8(&region, algorithm_object,
				(const uint8_t *)"hash", 4,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "algorithm.hash");
		}
		if (jsval_object_get_utf8(&region, prop, (const uint8_t *)"name", 4,
				&result) < 0) {
			return generated_fail_errno(detail, cap, "algorithm.hash.name");
		}
		status = generated_expect_string(&region, result,
				(const uint8_t *)"SHA-256", 7, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_object_get_utf8(&region, algorithm_object,
				(const uint8_t *)"length", 6,
				&result) < 0) {
			return generated_fail_errno(detail, cap, "algorithm.length");
		}
		if (result.kind != JSVAL_KIND_NUMBER || result.as.number != 512.0) {
			return generated_failf(detail, cap,
					"expected generated HMAC key length 512");
		}
		if (jsval_crypto_key_usages(&region, generated_key, &usages) < 0
				|| usages != (JSVAL_CRYPTO_KEY_USAGE_SIGN
					| JSVAL_CRYPTO_KEY_USAGE_VERIFY)) {
			return generated_fail_errno(detail, cap,
					"jsval_crypto_key_usages(generated)");
		}
		if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				generated_key, &exported_raw_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_export_key(raw)");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_microtask_drain(export raw)");
		}
		if (jsval_promise_result(&region, exported_raw_promise, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_promise_result(export raw)");
		}
		if (result.kind != JSVAL_KIND_ARRAY_BUFFER
				|| jsval_array_buffer_byte_length(&region, result, &len) < 0
				|| len != 64) {
			return generated_failf(detail, cap,
					"expected 64-byte exported raw HMAC key");
		}
		if (jsval_subtle_crypto_sign(&region, subtle_a, hmac_name, generated_key,
				digest_input_buffer, &sign_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_sign");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, sign_promise, &result) < 0) {
			return generated_fail_errno(detail, cap, "sign drain/result");
		}
		if (result.kind != JSVAL_KIND_ARRAY_BUFFER
				|| jsval_array_buffer_byte_length(&region, result, &len) < 0
				|| len != 32) {
			return generated_failf(detail, cap,
					"expected 32-byte HMAC signature");
		}
		if (jsval_subtle_crypto_verify(&region, subtle_a, hmac_name, generated_key,
				result, digest_input_buffer, &verify_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_verify(true)");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, verify_promise, &prop) < 0) {
			return generated_fail_errno(detail, cap, "verify(true) drain/result");
		}
		if (prop.kind != JSVAL_KIND_BOOL || !prop.as.boolean) {
			return generated_failf(detail, cap,
					"expected HMAC verify(true) to fulfill true");
		}
		if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 15,
				&typed_u8) < 0
				|| jsval_typed_array_buffer(&region, typed_u8, &prop) < 0
				|| jsval_subtle_crypto_verify(&region, subtle_a, hmac_name,
					generated_key, result, prop, &verify_false_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"verify(false) setup");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, verify_false_promise, &prop) < 0) {
			return generated_fail_errno(detail, cap,
					"verify(false) drain/result");
		}
		if (prop.kind != JSVAL_KIND_BOOL || prop.as.boolean) {
			return generated_failf(detail, cap,
					"expected HMAC verify(false) to fulfill false");
		}
		if (jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				generated_key, &exported_jwk_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_export_key(jwk)");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, exported_jwk_promise, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"export jwk drain/result");
		}
		if (jsval_object_get_utf8(&region, result, (const uint8_t *)"kty", 3,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "export jwk kty");
		}
		status = generated_expect_string(&region, prop, (const uint8_t *)"oct", 3,
				detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_object_get_utf8(&region, result, (const uint8_t *)"alg", 3,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "export jwk alg");
		}
		status = generated_expect_string(&region, prop,
				(const uint8_t *)"HS256", 5, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_object_get_utf8(&region, result, (const uint8_t *)"key_ops", 7,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "export jwk key_ops");
		}
		status = generated_expect_string_array(&region, prop,
				expected_sign_verify_ops, 2, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 32,
				&zero_key_input) < 0
				|| jsval_array_new(&region, 1, &sign_usages) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
					&sign_usage) < 0
				|| jsval_array_push(&region, sign_usages, sign_usage) < 0) {
			return generated_fail_errno(detail, cap, "raw import setup");
		}
		if (jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
				zero_key_input, hmac_algorithm, 1, sign_usages,
				&raw_import_key_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_import_key(raw)");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, raw_import_key_promise,
					&raw_import_key) < 0) {
			return generated_fail_errno(detail, cap,
					"raw import drain/result");
		}
		if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				raw_import_key, &exported_raw_promise) < 0) {
			return generated_fail_errno(detail, cap, "export raw imported");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, exported_raw_promise, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"export raw imported drain/result");
		}
		status = generated_expect_array_buffer_bytes(&region, result, zero_key_32,
				sizeof(zero_key_32), detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_object_new(&region, 5, &jwk_object) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"oct", 3,
					&result) < 0
				|| jsval_object_set_utf8(&region, jwk_object,
					(const uint8_t *)"kty", 3, result) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"__8", 3,
					&result) < 0
				|| jsval_object_set_utf8(&region, jwk_object,
					(const uint8_t *)"k", 1, result) < 0
				|| jsval_string_new_utf8(&region, (const uint8_t *)"HS256", 5,
					&result) < 0
				|| jsval_object_set_utf8(&region, jwk_object,
					(const uint8_t *)"alg", 3, result) < 0
				|| jsval_object_set_utf8(&region, jwk_object,
					(const uint8_t *)"ext", 3, jsval_bool(1)) < 0
				|| jsval_object_set_utf8(&region, jwk_object,
					(const uint8_t *)"key_ops", 7, sign_usages) < 0
				|| jsval_object_set_utf8(&region, hmac_algorithm,
					(const uint8_t *)"length", 6, jsval_number(9.0)) < 0) {
			return generated_fail_errno(detail, cap, "jwk import setup");
		}
		if (jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
				jwk_object, hmac_algorithm, 1, sign_usages,
				&jwk_import_key_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"jsval_subtle_crypto_import_key(jwk)");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, jwk_import_key_promise,
					&jwk_import_key) < 0) {
			return generated_fail_errno(detail, cap, "jwk import drain/result");
		}
		if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				jwk_import_key, &jwk_export_raw_promise) < 0) {
			return generated_fail_errno(detail, cap, "jwk export raw");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, jwk_export_raw_promise, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"jwk export raw drain/result");
		}
		status = generated_expect_array_buffer_bytes(&region, result,
				masked_key_9_bits, sizeof(masked_key_9_bits), detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
				jwk_import_key, &jwk_export_jwk_promise) < 0) {
			return generated_fail_errno(detail, cap, "jwk export jwk");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, jwk_export_jwk_promise, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"jwk export jwk drain/result");
		}
		if (jsval_object_get_utf8(&region, result, (const uint8_t *)"k", 1,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "jwk export jwk.k");
		}
		status = generated_expect_string(&region, prop, (const uint8_t *)"_4A", 3,
				detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_object_get_utf8(&region, result, (const uint8_t *)"key_ops", 7,
				&prop) < 0) {
			return generated_fail_errno(detail, cap, "jwk export jwk.key_ops");
		}
		status = generated_expect_string_array(&region, prop, expected_sign_ops, 1,
				detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_subtle_crypto_generate_key(&region, subtle_a, hmac_algorithm, 0,
				sign_usages, &non_extractable_key_promise) < 0) {
			return generated_fail_errno(detail, cap,
					"generate nonextractable key");
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(&region, &error) < 0
				|| jsval_promise_result(&region, non_extractable_key_promise,
					&non_extractable_key) < 0) {
			return generated_fail_errno(detail, cap,
					"nonextractable generate drain/result");
		}
		if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
				non_extractable_key, &non_extractable_export_promise) < 0
				|| jsval_promise_result(&region, non_extractable_export_promise,
					&dom_exception) < 0) {
			return generated_fail_errno(detail, cap,
					"nonextractable export result");
		}
		if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
			return generated_fail_errno(detail, cap,
					"nonextractable export exception name");
		}
		status = generated_expect_string(&region, result,
				(const uint8_t *)"InvalidAccessError", 18, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_array_new(&region, 0, &empty_usages) < 0
				|| jsval_subtle_crypto_generate_key(&region, subtle_a,
					hmac_algorithm, 1, empty_usages,
					&empty_usages_promise) < 0
				|| jsval_promise_result(&region, empty_usages_promise,
					&dom_exception) < 0
				|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
			return generated_fail_errno(detail, cap, "empty usages result");
		}
		status = generated_expect_string(&region, result,
				(const uint8_t *)"SyntaxError", 11, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
		if (jsval_string_new_utf8(&region, (const uint8_t *)"pkcs8", 5,
				&result) < 0
				|| jsval_subtle_crypto_import_key(&region, subtle_a, result,
					zero_key_input, hmac_algorithm, 1, sign_usages,
					&unsupported_format_promise) < 0
				|| jsval_promise_result(&region, unsupported_format_promise,
					&dom_exception) < 0
				|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
			return generated_fail_errno(detail, cap, "unsupported format result");
		}
			status = generated_expect_string(&region, result,
					(const uint8_t *)"NotSupportedError", 17, detail, cap);
			if (status != GENERATED_PASS) {
				return status;
			}

			{
				jsval_t aes_usages;
				jsval_t aes_algorithm;
				jsval_t aes_import_algorithm;
				jsval_t aes_name;
				jsval_t aes_raw_key_promise;
				jsval_t aes_raw_key;
				jsval_t aes_export_raw_promise;
				jsval_t aes_export_jwk_promise;
				jsval_t aes_import_raw_promise;
				jsval_t aes_import_raw_key;
				jsval_t aes_import_jwk_promise;
				jsval_t aes_import_jwk_key;
				jsval_t aes_encrypt_promise;
				jsval_t aes_decrypt_promise;
				jsval_t aes_bad_decrypt_key_promise;
				jsval_t aes_bad_decrypt_key;
				jsval_t aes_bad_decrypt_promise;
				jsval_t aes_invalid_iv_params;
				jsval_t aes_invalid_iv_promise;
				jsval_t aes_invalid_tag_params;
				jsval_t aes_invalid_tag_promise;
				jsval_t aes_non_extractable_promise;
				jsval_t aes_non_extractable_key;
				jsval_t aes_non_extractable_export_promise;
				jsval_t aes_iv;
				jsval_t aes_iv_buffer;
				jsval_t aes_params;
				jsval_t aes_bad_format;
				jsval_t aes_unsupported_format_promise;
				jsval_t aes_jwk_object;
				jsval_t aes_malformed_jwk_promise;

				if (jsval_array_new(&region, 2, &aes_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"encrypt", 7, &result) < 0
						|| jsval_array_push(&region, aes_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"decrypt", 7, &result) < 0
						|| jsval_array_push(&region, aes_usages, result) < 0
						|| jsval_object_new(&region, 2, &aes_algorithm) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AES-GCM", 7, &aes_name) < 0
						|| jsval_object_set_utf8(&region, aes_algorithm,
							(const uint8_t *)"name", 4, aes_name) < 0
						|| jsval_object_set_utf8(&region, aes_algorithm,
							(const uint8_t *)"length", 6, jsval_number(256.0)) < 0
						|| jsval_object_new(&region, 1, &aes_import_algorithm) < 0
						|| jsval_object_set_utf8(&region, aes_import_algorithm,
							(const uint8_t *)"name", 4, aes_name) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
							&aes_iv) < 0
						|| jsval_typed_array_buffer(&region, aes_iv, &aes_iv_buffer) < 0
						|| jsval_object_new(&region, 2, &aes_params) < 0
						|| jsval_object_set_utf8(&region, aes_params,
							(const uint8_t *)"name", 4, aes_name) < 0
						|| jsval_object_set_utf8(&region, aes_params,
							(const uint8_t *)"iv", 2, aes_iv_buffer) < 0) {
					return generated_fail_errno(detail, cap, "aes-gcm setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a, aes_algorithm,
						1, aes_usages, &aes_raw_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(aes)");
				}
				if (jsval_promise_state(&region, aes_raw_key_promise, &promise_state)
						< 0
						|| promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected AES-GCM generateKey promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_raw_key_promise,
							&aes_raw_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes generateKey drain/result");
				}
				if (aes_raw_key.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected AES-GCM generateKey to resolve CryptoKey");
				}
				if (jsval_crypto_key_algorithm(&region, aes_raw_key, &result) < 0
						|| result.kind != JSVAL_KIND_OBJECT) {
					return generated_fail_errno(detail, cap,
							"jsval_crypto_key_algorithm(aes generated)");
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"name", 4, &prop) < 0) {
					return generated_fail_errno(detail, cap, "aes algorithm.name");
				}
				status = generated_expect_string(&region, prop,
						(const uint8_t *)"AES-GCM", 7, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"length", 6, &prop) < 0) {
					return generated_fail_errno(detail, cap, "aes algorithm.length");
				}
				if (prop.kind != JSVAL_KIND_NUMBER || prop.as.number != 256.0) {
					return generated_failf(detail, cap,
							"expected generated AES-GCM key length 256");
				}
				if (jsval_crypto_key_usages(&region, aes_raw_key, &usages) < 0
						|| usages != (JSVAL_CRYPTO_KEY_USAGE_ENCRYPT
							| JSVAL_CRYPTO_KEY_USAGE_DECRYPT)) {
					return generated_fail_errno(detail, cap,
							"jsval_crypto_key_usages(aes generated)");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						aes_raw_key, &aes_export_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_export_key(aes raw)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_export_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes export raw drain/result");
				}
				if (result.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, result, &len) < 0
						|| len != 32) {
					return generated_failf(detail, cap,
							"expected 32-byte exported raw AES-GCM key");
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
						&result) < 0
						|| jsval_typed_array_buffer(&region, result, &prop) < 0
						|| jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
							prop, aes_import_algorithm, 1, aes_usages,
							&aes_import_raw_promise) < 0) {
					return generated_fail_errno(detail, cap, "aes raw import setup");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_import_raw_promise,
							&aes_import_raw_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes raw import drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						aes_import_raw_key,
						&aes_export_raw_promise) < 0) {
					return generated_fail_errno(detail, cap, "aes raw re-export");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_export_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes raw re-export drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						zero_key_16, sizeof(zero_key_16), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 5, &aes_jwk_object) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"oct", 3,
							&result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"kty", 3, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAA", 22,
							&result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"k", 1, result) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"A128GCM",
							7, &result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"alg", 3, result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"ext", 3, jsval_bool(1)) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"key_ops", 7, aes_usages) < 0) {
					return generated_fail_errno(detail, cap, "aes jwk setup");
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
						aes_jwk_object, aes_import_algorithm, 1, aes_usages,
						&aes_import_jwk_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_import_key(aes jwk)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_import_jwk_promise,
							&aes_import_jwk_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes jwk import drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, jwk_format,
						aes_import_jwk_key, &aes_export_jwk_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_export_key(aes jwk)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_export_jwk_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes jwk export drain/result");
				}
				if (jsval_object_get_utf8(&region, result, (const uint8_t *)"alg", 3,
						&prop) < 0) {
					return generated_fail_errno(detail, cap, "aes jwk alg");
				}
				status = generated_expect_string(&region, prop,
						(const uint8_t *)"A128GCM", 7, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"key_ops", 7, &prop) < 0) {
					return generated_fail_errno(detail, cap, "aes jwk key_ops");
				}
				status = generated_expect_string_array(&region, prop,
						expected_encrypt_decrypt_ops, 2, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_encrypt(&region, subtle_a, aes_params,
						aes_import_raw_key,
						digest_input_buffer, &aes_encrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_encrypt");
				}
				if (jsval_promise_state(&region, aes_encrypt_promise, &promise_state) < 0
						|| promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected AES-GCM encrypt promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_encrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes encrypt drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						aes_gcm_zero_ciphertext_16_tag,
						sizeof(aes_gcm_zero_ciphertext_16_tag), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_decrypt(&region, subtle_a, aes_params,
						aes_import_raw_key,
						result, &aes_decrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_decrypt");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_decrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes decrypt drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						zero_key_16, sizeof(zero_key_16), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a, aes_algorithm, 1,
						aes_usages, &aes_bad_decrypt_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes bad decrypt key generate");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_bad_decrypt_key_promise,
							&aes_bad_decrypt_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes bad decrypt key drain/result");
				}
				if (jsval_subtle_crypto_encrypt(&region, subtle_a, aes_params,
						aes_import_raw_key,
						digest_input_buffer, &aes_encrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes re-encrypt for bad decrypt");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_encrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes re-encrypt drain/result");
				}
				if (jsval_subtle_crypto_decrypt(&region, subtle_a, aes_params,
						aes_bad_decrypt_key, result, &aes_bad_decrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes bad decrypt call");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_bad_decrypt_promise,
							&dom_exception) < 0) {
					return generated_fail_errno(detail, cap,
							"aes bad decrypt drain/result");
				}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes bad decrypt exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"OperationError", 14, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 1, &aes_invalid_iv_params) < 0
						|| jsval_object_set_utf8(&region, aes_invalid_iv_params,
							(const uint8_t *)"name", 4, aes_name) < 0
						|| jsval_subtle_crypto_encrypt(&region, subtle_a,
							aes_invalid_iv_params, aes_import_raw_key,
							digest_input_buffer,
							&aes_invalid_iv_promise) < 0
						|| jsval_promise_result(&region, aes_invalid_iv_promise,
							&dom_exception) < 0) {
						return generated_fail_errno(detail, cap, "aes invalid iv");
					}
					if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes invalid iv exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"TypeError", 9, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 3, &aes_invalid_tag_params) < 0
						|| jsval_object_set_utf8(&region, aes_invalid_tag_params,
							(const uint8_t *)"name", 4, aes_name) < 0
						|| jsval_object_set_utf8(&region, aes_invalid_tag_params,
							(const uint8_t *)"iv", 2, aes_iv_buffer) < 0
						|| jsval_object_set_utf8(&region, aes_invalid_tag_params,
							(const uint8_t *)"tagLength", 9,
							jsval_number(40.0)) < 0
						|| jsval_subtle_crypto_encrypt(&region, subtle_a,
							aes_invalid_tag_params, aes_import_raw_key,
							digest_input_buffer,
							&aes_invalid_tag_promise) < 0
						|| jsval_promise_result(&region, aes_invalid_tag_promise,
							&dom_exception) < 0) {
						return generated_fail_errno(detail, cap, "aes invalid tag");
					}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes invalid tag exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"OperationError", 14, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a, aes_algorithm, 0,
						aes_usages, &aes_non_extractable_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes nonextractable generate");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, aes_non_extractable_promise,
							&aes_non_extractable_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes nonextractable drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						aes_non_extractable_key,
						&aes_non_extractable_export_promise) < 0
						|| jsval_promise_result(&region,
							aes_non_extractable_export_promise, &dom_exception) < 0) {
					return generated_fail_errno(detail, cap,
							"aes nonextractable export");
				}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes nonextractable exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"InvalidAccessError", 18, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_string_new_utf8(&region, (const uint8_t *)"pkcs8", 5,
						&aes_bad_format) < 0
						|| jsval_subtle_crypto_import_key(&region, subtle_a,
							aes_bad_format, aes_iv_buffer, aes_import_algorithm, 1,
							aes_usages, &aes_unsupported_format_promise) < 0
						|| jsval_promise_result(&region,
							aes_unsupported_format_promise, &dom_exception) < 0) {
					return generated_fail_errno(detail, cap,
							"aes unsupported format");
				}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes unsupported format exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"NotSupportedError", 17, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 5, &aes_jwk_object) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"RSA", 3,
							&result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"kty", 3, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAA", 22,
							&result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"k", 1, result) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"A128GCM",
							7, &result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"alg", 3, result) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"ext", 3, jsval_bool(1)) < 0
						|| jsval_object_set_utf8(&region, aes_jwk_object,
							(const uint8_t *)"key_ops", 7, aes_usages) < 0
						|| jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
							aes_jwk_object, aes_import_algorithm, 1, aes_usages,
							&aes_malformed_jwk_promise) < 0
						|| jsval_promise_result(&region, aes_malformed_jwk_promise,
							&dom_exception) < 0) {
					return generated_fail_errno(detail, cap, "aes malformed jwk");
				}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes malformed jwk exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"DataError", 9, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}

				/*
				 * AES-GCM as a wrapping cipher: wrap an HMAC inner
				 * key with an AES-GCM wrapping key, unwrap it back,
				 * and verify the algorithm name round-trips.
				 */
				{
					jsval_t gcm_wrap_usages;
					jsval_t gcm_wrap_alg;
					jsval_t gcm_wrap_key_promise;
					jsval_t gcm_wrap_key;
					jsval_t hmac_inner_alg;
					jsval_t hmac_inner_hash_obj;
					jsval_t hmac_inner_usages;
					jsval_t hmac_inner_promise;
					jsval_t hmac_inner_key;
					jsval_t gcm_wrap_promise;
					jsval_t gcm_wrapped;
					jsval_t gcm_unwrap_promise;
					jsval_t gcm_unwrapped_key;

					if (jsval_array_new(&region, 2, &gcm_wrap_usages) < 0
							|| jsval_string_new_utf8(&region,
								(const uint8_t *)"wrapKey", 7, &result) < 0
							|| jsval_array_push(&region, gcm_wrap_usages,
								result) < 0
							|| jsval_string_new_utf8(&region,
								(const uint8_t *)"unwrapKey", 9, &result) < 0
							|| jsval_array_push(&region, gcm_wrap_usages,
								result) < 0
							|| jsval_object_new(&region, 2, &gcm_wrap_alg) < 0
							|| jsval_object_set_utf8(&region, gcm_wrap_alg,
								(const uint8_t *)"name", 4, aes_name) < 0
							|| jsval_object_set_utf8(&region, gcm_wrap_alg,
								(const uint8_t *)"length", 6,
								jsval_number(128.0)) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap setup");
					}
					if (jsval_subtle_crypto_generate_key(&region, subtle_a,
							gcm_wrap_alg, 1, gcm_wrap_usages,
							&gcm_wrap_key_promise) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap generate");
					}
					memset(&error, 0, sizeof(error));
					if (jsval_microtask_drain(&region, &error) < 0
							|| jsval_promise_result(&region,
								gcm_wrap_key_promise, &gcm_wrap_key) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap drain");
					}
					if (jsval_object_new(&region, 1,
							&hmac_inner_hash_obj) < 0
							|| jsval_string_new_utf8(&region,
								(const uint8_t *)"SHA-256", 7, &result) < 0
							|| jsval_object_set_utf8(&region,
								hmac_inner_hash_obj,
								(const uint8_t *)"name", 4, result) < 0
							|| jsval_object_new(&region, 2,
								&hmac_inner_alg) < 0
							|| jsval_string_new_utf8(&region,
								(const uint8_t *)"HMAC", 4, &result) < 0
							|| jsval_object_set_utf8(&region, hmac_inner_alg,
								(const uint8_t *)"name", 4, result) < 0
							|| jsval_object_set_utf8(&region, hmac_inner_alg,
								(const uint8_t *)"hash", 4,
								hmac_inner_hash_obj) < 0
							|| jsval_array_new(&region, 2,
								&hmac_inner_usages) < 0
							|| jsval_string_new_utf8(&region,
								(const uint8_t *)"sign", 4, &result) < 0
							|| jsval_array_push(&region, hmac_inner_usages,
								result) < 0
							|| jsval_string_new_utf8(&region,
								(const uint8_t *)"verify", 6, &result) < 0
							|| jsval_array_push(&region, hmac_inner_usages,
								result) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap inner setup");
					}
					if (jsval_subtle_crypto_generate_key(&region, subtle_a,
							hmac_inner_alg, 1, hmac_inner_usages,
							&hmac_inner_promise) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap inner generate");
					}
					memset(&error, 0, sizeof(error));
					if (jsval_microtask_drain(&region, &error) < 0
							|| jsval_promise_result(&region,
								hmac_inner_promise,
								&hmac_inner_key) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap inner drain");
					}
					if (jsval_subtle_crypto_wrap_key(&region, subtle_a,
							raw_format, hmac_inner_key, gcm_wrap_key,
							aes_params, &gcm_wrap_promise) < 0) {
						return generated_fail_errno(detail, cap,
								"jsval_subtle_crypto_wrap_key(aes-gcm)");
					}
					memset(&error, 0, sizeof(error));
					if (jsval_microtask_drain(&region, &error) < 0
							|| jsval_promise_result(&region,
								gcm_wrap_promise, &gcm_wrapped) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm wrap drain/result");
					}
					if (gcm_wrapped.kind != JSVAL_KIND_ARRAY_BUFFER
							|| jsval_array_buffer_byte_length(&region,
								gcm_wrapped, &len) < 0
							|| len != 80) {
						return generated_failf(detail, cap,
								"expected 80-byte AES-GCM wrap of HMAC-256");
					}
					if (jsval_subtle_crypto_unwrap_key(&region, subtle_a,
							raw_format, gcm_wrapped, gcm_wrap_key,
							aes_params, hmac_inner_alg, 1, hmac_inner_usages,
							&gcm_unwrap_promise) < 0) {
						return generated_fail_errno(detail, cap,
								"jsval_subtle_crypto_unwrap_key(aes-gcm)");
					}
					memset(&error, 0, sizeof(error));
					if (jsval_microtask_drain(&region, &error) < 0
							|| jsval_promise_result(&region,
								gcm_unwrap_promise,
								&gcm_unwrapped_key) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm unwrap drain/result");
					}
					if (gcm_unwrapped_key.kind != JSVAL_KIND_CRYPTO_KEY) {
						return generated_failf(detail, cap,
								"expected unwrapped HMAC CryptoKey");
					}
					if (jsval_crypto_key_algorithm(&region,
							gcm_unwrapped_key, &result) < 0
							|| jsval_object_get_utf8(&region, result,
								(const uint8_t *)"name", 4, &prop) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-gcm unwrap algorithm");
					}
					status = generated_expect_string(&region, prop,
							(const uint8_t *)"HMAC", 4, detail, cap);
					if (status != GENERATED_PASS) {
						return status;
					}
				}
			}

			{
				/* AES-CTR transpiled smoke: generateKey, encrypt/decrypt
				 * round-trip on a zero plaintext, exportKey raw. */
				jsval_t ctr_usages;
				jsval_t ctr_algorithm;
				jsval_t ctr_import_algorithm;
				jsval_t ctr_params;
				jsval_t ctr_name;
				jsval_t ctr_counter;
				jsval_t ctr_counter_buffer;
				jsval_t ctr_generated_promise;
				jsval_t ctr_generated_key;
				jsval_t ctr_export_raw_promise;
				jsval_t ctr_encrypt_promise;
				jsval_t ctr_decrypt_promise;
				jsval_t ctr_bad_counter_params;
				jsval_t ctr_bad_counter;
				jsval_t ctr_bad_counter_buffer;
				jsval_t ctr_bad_counter_promise;

				if (jsval_array_new(&region, 2, &ctr_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"encrypt", 7, &result) < 0
						|| jsval_array_push(&region, ctr_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"decrypt", 7, &result) < 0
						|| jsval_array_push(&region, ctr_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AES-CTR", 7, &ctr_name) < 0
						|| jsval_object_new(&region, 2, &ctr_algorithm) < 0
						|| jsval_object_set_utf8(&region, ctr_algorithm,
							(const uint8_t *)"name", 4, ctr_name) < 0
						|| jsval_object_set_utf8(&region, ctr_algorithm,
							(const uint8_t *)"length", 6, jsval_number(128.0)) < 0
						|| jsval_object_new(&region, 1, &ctr_import_algorithm) < 0
						|| jsval_object_set_utf8(&region, ctr_import_algorithm,
							(const uint8_t *)"name", 4, ctr_name) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
							16, &ctr_counter) < 0
						|| jsval_typed_array_buffer(&region, ctr_counter,
							&ctr_counter_buffer) < 0
						|| jsval_object_new(&region, 3, &ctr_params) < 0
						|| jsval_object_set_utf8(&region, ctr_params,
							(const uint8_t *)"name", 4, ctr_name) < 0
						|| jsval_object_set_utf8(&region, ctr_params,
							(const uint8_t *)"counter", 7, ctr_counter_buffer) < 0
						|| jsval_object_set_utf8(&region, ctr_params,
							(const uint8_t *)"length", 6, jsval_number(128.0)) < 0) {
					return generated_fail_errno(detail, cap, "aes-ctr setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						ctr_algorithm, 1, ctr_usages,
						&ctr_generated_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(aes-ctr)");
				}
				if (jsval_promise_state(&region, ctr_generated_promise,
						&promise_state) < 0
						|| promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected AES-CTR generateKey promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, ctr_generated_promise,
							&ctr_generated_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr generateKey drain/result");
				}
				if (ctr_generated_key.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected AES-CTR generateKey to resolve CryptoKey");
				}
				if (jsval_crypto_key_algorithm(&region, ctr_generated_key,
						&result) < 0
						|| result.kind != JSVAL_KIND_OBJECT) {
					return generated_fail_errno(detail, cap,
							"aes-ctr key algorithm");
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"name", 4, &prop) < 0) {
					return generated_fail_errno(detail, cap, "aes-ctr name");
				}
				status = generated_expect_string(&region, prop,
						(const uint8_t *)"AES-CTR", 7, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						ctr_generated_key, &ctr_export_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr exportKey raw");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, ctr_export_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr export raw drain/result");
				}
				if (result.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, result, &len) < 0
						|| len != 16) {
					return generated_failf(detail, cap,
							"expected 16-byte exported AES-CTR raw key");
				}
				if (jsval_subtle_crypto_encrypt(&region, subtle_a, ctr_params,
						ctr_generated_key, digest_input_buffer,
						&ctr_encrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_encrypt(aes-ctr)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, ctr_encrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr encrypt drain/result");
				}
				if (result.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, result, &len) < 0
						|| len != 16) {
					return generated_failf(detail, cap,
							"expected 16-byte AES-CTR ciphertext");
				}
				if (jsval_subtle_crypto_decrypt(&region, subtle_a, ctr_params,
						ctr_generated_key, result, &ctr_decrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_decrypt(aes-ctr)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, ctr_decrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr decrypt drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						zero_key_16, sizeof(zero_key_16), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
						&ctr_bad_counter) < 0
						|| jsval_typed_array_buffer(&region, ctr_bad_counter,
							&ctr_bad_counter_buffer) < 0
						|| jsval_object_new(&region, 3,
							&ctr_bad_counter_params) < 0
						|| jsval_object_set_utf8(&region, ctr_bad_counter_params,
							(const uint8_t *)"name", 4, ctr_name) < 0
						|| jsval_object_set_utf8(&region, ctr_bad_counter_params,
							(const uint8_t *)"counter", 7, ctr_bad_counter_buffer) < 0
						|| jsval_object_set_utf8(&region, ctr_bad_counter_params,
							(const uint8_t *)"length", 6, jsval_number(64.0)) < 0
						|| jsval_subtle_crypto_encrypt(&region, subtle_a,
							ctr_bad_counter_params, ctr_generated_key,
							digest_input_buffer, &ctr_bad_counter_promise) < 0
						|| jsval_promise_result(&region, ctr_bad_counter_promise,
							&dom_exception) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr bad counter setup");
				}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-ctr bad counter exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"OperationError", 14, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
			}

			{
				/* AES-CBC transpiled smoke: generateKey, exportKey raw,
				 * encrypt/decrypt round-trip on a zero plaintext, invalid
				 * iv length rejection. */
				jsval_t cbc_usages;
				jsval_t cbc_algorithm;
				jsval_t cbc_import_algorithm;
				jsval_t cbc_params;
				jsval_t cbc_name;
				jsval_t cbc_iv;
				jsval_t cbc_iv_buffer;
				jsval_t cbc_generated_promise;
				jsval_t cbc_generated_key;
				jsval_t cbc_export_raw_promise;
				jsval_t cbc_encrypt_promise;
				jsval_t cbc_decrypt_promise;
				jsval_t cbc_bad_iv_params;
				jsval_t cbc_bad_iv;
				jsval_t cbc_bad_iv_buffer;
				jsval_t cbc_bad_iv_promise;

				if (jsval_array_new(&region, 2, &cbc_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"encrypt", 7, &result) < 0
						|| jsval_array_push(&region, cbc_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"decrypt", 7, &result) < 0
						|| jsval_array_push(&region, cbc_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AES-CBC", 7, &cbc_name) < 0
						|| jsval_object_new(&region, 2, &cbc_algorithm) < 0
						|| jsval_object_set_utf8(&region, cbc_algorithm,
							(const uint8_t *)"name", 4, cbc_name) < 0
						|| jsval_object_set_utf8(&region, cbc_algorithm,
							(const uint8_t *)"length", 6, jsval_number(128.0)) < 0
						|| jsval_object_new(&region, 1, &cbc_import_algorithm) < 0
						|| jsval_object_set_utf8(&region, cbc_import_algorithm,
							(const uint8_t *)"name", 4, cbc_name) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
							16, &cbc_iv) < 0
						|| jsval_typed_array_buffer(&region, cbc_iv,
							&cbc_iv_buffer) < 0
						|| jsval_object_new(&region, 2, &cbc_params) < 0
						|| jsval_object_set_utf8(&region, cbc_params,
							(const uint8_t *)"name", 4, cbc_name) < 0
						|| jsval_object_set_utf8(&region, cbc_params,
							(const uint8_t *)"iv", 2, cbc_iv_buffer) < 0) {
					return generated_fail_errno(detail, cap, "aes-cbc setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						cbc_algorithm, 1, cbc_usages,
						&cbc_generated_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(aes-cbc)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, cbc_generated_promise,
							&cbc_generated_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc generateKey drain/result");
				}
				if (cbc_generated_key.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected AES-CBC generateKey to resolve CryptoKey");
				}
				if (jsval_crypto_key_algorithm(&region, cbc_generated_key,
						&result) < 0
						|| result.kind != JSVAL_KIND_OBJECT) {
					return generated_fail_errno(detail, cap,
							"aes-cbc key algorithm");
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"name", 4, &prop) < 0) {
					return generated_fail_errno(detail, cap, "aes-cbc name");
				}
				status = generated_expect_string(&region, prop,
						(const uint8_t *)"AES-CBC", 7, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						cbc_generated_key, &cbc_export_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc exportKey raw");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, cbc_export_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc export raw drain/result");
				}
				if (result.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, result, &len) < 0
						|| len != 16) {
					return generated_failf(detail, cap,
							"expected 16-byte exported AES-CBC raw key");
				}
				if (jsval_subtle_crypto_encrypt(&region, subtle_a, cbc_params,
						cbc_generated_key, digest_input_buffer,
						&cbc_encrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_encrypt(aes-cbc)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, cbc_encrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc encrypt drain/result");
				}
				if (result.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, result, &len) < 0
						|| len != 32) {
					return generated_failf(detail, cap,
							"expected 32-byte AES-CBC ciphertext");
				}
				if (jsval_subtle_crypto_decrypt(&region, subtle_a, cbc_params,
						cbc_generated_key, result, &cbc_decrypt_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_decrypt(aes-cbc)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, cbc_decrypt_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc decrypt drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						zero_key_16, sizeof(zero_key_16), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 12,
						&cbc_bad_iv) < 0
						|| jsval_typed_array_buffer(&region, cbc_bad_iv,
							&cbc_bad_iv_buffer) < 0
						|| jsval_object_new(&region, 2, &cbc_bad_iv_params) < 0
						|| jsval_object_set_utf8(&region, cbc_bad_iv_params,
							(const uint8_t *)"name", 4, cbc_name) < 0
						|| jsval_object_set_utf8(&region, cbc_bad_iv_params,
							(const uint8_t *)"iv", 2, cbc_bad_iv_buffer) < 0
						|| jsval_subtle_crypto_encrypt(&region, subtle_a,
							cbc_bad_iv_params, cbc_generated_key,
							digest_input_buffer, &cbc_bad_iv_promise) < 0
						|| jsval_promise_result(&region, cbc_bad_iv_promise,
							&dom_exception) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc bad iv setup");
				}
				if (jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-cbc bad iv exception name");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"OperationError", 14, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
			}

			{
				/* AES-KW transpiled smoke: generateKey + wrap an AES-GCM
				 * key with the AES-KW key, then unwrap it back. */
				jsval_t kw_usages;
				jsval_t kw_algorithm;
				jsval_t kw_name;
				jsval_t kw_generated_promise;
				jsval_t kw_generated_key;
				jsval_t inner_aes_alg;
				jsval_t inner_aes_usages;
				jsval_t inner_aes_promise;
				jsval_t inner_aes_key;
				jsval_t kw_wrap_promise;
				jsval_t kw_wrapped;
				jsval_t kw_unwrap_promise;
				jsval_t kw_unwrapped_key;

				if (jsval_array_new(&region, 2, &kw_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"wrapKey", 7, &result) < 0
						|| jsval_array_push(&region, kw_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"unwrapKey", 9, &result) < 0
						|| jsval_array_push(&region, kw_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AES-KW", 6, &kw_name) < 0
						|| jsval_object_new(&region, 2, &kw_algorithm) < 0
						|| jsval_object_set_utf8(&region, kw_algorithm,
							(const uint8_t *)"name", 4, kw_name) < 0
						|| jsval_object_set_utf8(&region, kw_algorithm,
							(const uint8_t *)"length", 6, jsval_number(128.0)) < 0) {
					return generated_fail_errno(detail, cap, "aes-kw setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						kw_algorithm, 1, kw_usages,
						&kw_generated_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(aes-kw)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, kw_generated_promise,
							&kw_generated_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-kw generateKey drain/result");
				}
				if (kw_generated_key.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected AES-KW generateKey to resolve CryptoKey");
				}
				if (jsval_array_new(&region, 2, &inner_aes_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"encrypt", 7, &result) < 0
						|| jsval_array_push(&region, inner_aes_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"decrypt", 7, &result) < 0
						|| jsval_array_push(&region, inner_aes_usages, result) < 0
						|| jsval_object_new(&region, 2, &inner_aes_alg) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"AES-GCM", 7, &result) < 0
						|| jsval_object_set_utf8(&region, inner_aes_alg,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, inner_aes_alg,
							(const uint8_t *)"length", 6,
							jsval_number(128.0)) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-kw inner aes-gcm setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						inner_aes_alg, 1, inner_aes_usages,
						&inner_aes_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-kw inner generateKey");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, inner_aes_promise,
							&inner_aes_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-kw inner drain/result");
				}
				if (jsval_subtle_crypto_wrap_key(&region, subtle_a, raw_format,
						inner_aes_key, kw_generated_key, kw_algorithm,
						&kw_wrap_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_wrap_key");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, kw_wrap_promise,
							&kw_wrapped) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-kw wrap drain/result");
				}
				if (kw_wrapped.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, kw_wrapped,
							&len) < 0
						|| len != 24) {
					return generated_failf(detail, cap,
							"expected 24-byte AES-KW wrap of an AES-128 key");
				}
				if (jsval_subtle_crypto_unwrap_key(&region, subtle_a, raw_format,
						kw_wrapped, kw_generated_key, kw_algorithm,
						inner_aes_alg, 1, inner_aes_usages,
						&kw_unwrap_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_unwrap_key");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, kw_unwrap_promise,
							&kw_unwrapped_key) < 0) {
					return generated_fail_errno(detail, cap,
							"aes-kw unwrap drain/result");
				}
				if (kw_unwrapped_key.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected unwrapped AES-GCM CryptoKey");
				}
				if (jsval_crypto_key_algorithm(&region, kw_unwrapped_key,
						&result) < 0
						|| result.kind != JSVAL_KIND_OBJECT) {
					return generated_fail_errno(detail, cap,
							"unwrapped AES-GCM key algorithm");
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"name", 4, &prop) < 0) {
					return generated_fail_errno(detail, cap,
							"unwrapped name");
				}
				status = generated_expect_string(&region, prop,
						(const uint8_t *)"AES-GCM", 7, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}

				/*
				 * JWK format round-trip: wrap the same inner AES-GCM
				 * key in JWK format, then unwrap it back and verify
				 * the algorithm metadata round-trips through the JWK.
				 */
				{
					jsval_t jwk_format_value;
					jsval_t kw_jwk_wrap_promise;
					jsval_t kw_jwk_wrapped;
					jsval_t kw_jwk_unwrap_promise;
					jsval_t kw_jwk_unwrapped_key;

					if (jsval_string_new_utf8(&region,
							(const uint8_t *)"jwk", 3, &jwk_format_value) < 0
							|| jsval_subtle_crypto_wrap_key(&region, subtle_a,
								jwk_format_value, inner_aes_key,
								kw_generated_key, kw_algorithm,
								&kw_jwk_wrap_promise) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-kw jwk wrap setup");
					}
					memset(&error, 0, sizeof(error));
					if (jsval_microtask_drain(&region, &error) < 0
							|| jsval_promise_result(&region,
								kw_jwk_wrap_promise, &kw_jwk_wrapped) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-kw jwk wrap drain/result");
					}
					if (kw_jwk_wrapped.kind != JSVAL_KIND_ARRAY_BUFFER) {
						return generated_failf(detail, cap,
								"expected AES-KW jwk wrap ArrayBuffer");
					}
					if (jsval_subtle_crypto_unwrap_key(&region, subtle_a,
							jwk_format_value, kw_jwk_wrapped, kw_generated_key,
							kw_algorithm, inner_aes_alg, 1, inner_aes_usages,
							&kw_jwk_unwrap_promise) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-kw jwk unwrap setup");
					}
					memset(&error, 0, sizeof(error));
					if (jsval_microtask_drain(&region, &error) < 0
							|| jsval_promise_result(&region,
								kw_jwk_unwrap_promise,
								&kw_jwk_unwrapped_key) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-kw jwk unwrap drain/result");
					}
					if (kw_jwk_unwrapped_key.kind != JSVAL_KIND_CRYPTO_KEY) {
						return generated_failf(detail, cap,
								"expected AES-KW jwk unwrap CryptoKey");
					}
					if (jsval_crypto_key_algorithm(&region,
							kw_jwk_unwrapped_key, &result) < 0
							|| result.kind != JSVAL_KIND_OBJECT
							|| jsval_object_get_utf8(&region, result,
								(const uint8_t *)"name", 4, &prop) < 0) {
						return generated_fail_errno(detail, cap,
								"aes-kw jwk unwrap name");
					}
					status = generated_expect_string(&region, prop,
							(const uint8_t *)"AES-GCM", 7, detail, cap);
					if (status != GENERATED_PASS) {
						return status;
					}
				}
			}

			{
				/*
				 * ECDSA P-256 transpiled smoke: generateKey returns a
				 * {publicKey, privateKey} object, sign a 16-byte
				 * payload with the private key, verify with the public
				 * key, and confirm the verify resolves to true.
				 */
				jsval_t ecdsa_usages;
				jsval_t ecdsa_algorithm;
				jsval_t ecdsa_name;
				jsval_t ecdsa_curve;
				jsval_t ecdsa_generate_promise;
				jsval_t ecdsa_keypair;
				jsval_t ecdsa_public;
				jsval_t ecdsa_private;
				jsval_t sign_algorithm;
				jsval_t sign_hash_name;
				jsval_t sign_data;
				jsval_t sign_data_buffer;
				jsval_t ecdsa_sign_promise;
				jsval_t ecdsa_signature;
				jsval_t ecdsa_verify_promise;

				if (jsval_array_new(&region, 2, &ecdsa_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"sign", 4, &result) < 0
						|| jsval_array_push(&region, ecdsa_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"verify", 6, &result) < 0
						|| jsval_array_push(&region, ecdsa_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"ECDSA", 5, &ecdsa_name) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"P-256", 5, &ecdsa_curve) < 0
						|| jsval_object_new(&region, 2, &ecdsa_algorithm) < 0
						|| jsval_object_set_utf8(&region, ecdsa_algorithm,
							(const uint8_t *)"name", 4, ecdsa_name) < 0
						|| jsval_object_set_utf8(&region, ecdsa_algorithm,
							(const uint8_t *)"namedCurve", 10,
							ecdsa_curve) < 0) {
					return generated_fail_errno(detail, cap, "ecdsa setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						ecdsa_algorithm, 1, ecdsa_usages,
						&ecdsa_generate_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(ECDSA)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region,
							ecdsa_generate_promise, &ecdsa_keypair) < 0) {
					return generated_fail_errno(detail, cap,
							"ecdsa generate drain/result");
				}
				if (ecdsa_keypair.kind != JSVAL_KIND_OBJECT) {
					return generated_failf(detail, cap,
							"expected ECDSA generateKey key pair object");
				}
				if (jsval_object_get_utf8(&region, ecdsa_keypair,
						(const uint8_t *)"publicKey", 9, &ecdsa_public) < 0
						|| jsval_object_get_utf8(&region, ecdsa_keypair,
							(const uint8_t *)"privateKey", 10,
							&ecdsa_private) < 0) {
					return generated_fail_errno(detail, cap,
							"ecdsa pair accessors");
				}
				if (ecdsa_public.kind != JSVAL_KIND_CRYPTO_KEY
						|| ecdsa_private.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected ECDSA pair to hold CryptoKey values");
				}
				if (jsval_object_new(&region, 2, &sign_algorithm) < 0
						|| jsval_object_set_utf8(&region, sign_algorithm,
							(const uint8_t *)"name", 4, ecdsa_name) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"SHA-256", 7,
							&sign_hash_name) < 0
						|| jsval_object_set_utf8(&region, sign_algorithm,
							(const uint8_t *)"hash", 4, sign_hash_name) < 0) {
					return generated_fail_errno(detail, cap,
							"ecdsa sign alg setup");
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 16,
						&sign_data) < 0
						|| jsval_typed_array_buffer(&region, sign_data,
							&sign_data_buffer) < 0) {
					return generated_fail_errno(detail, cap,
							"ecdsa data buffer");
				}
				if (jsval_subtle_crypto_sign(&region, subtle_a, sign_algorithm,
						ecdsa_private, sign_data_buffer,
						&ecdsa_sign_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_sign(ECDSA)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, ecdsa_sign_promise,
							&ecdsa_signature) < 0) {
					return generated_fail_errno(detail, cap,
							"ecdsa sign drain/result");
				}
				if (ecdsa_signature.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region,
							ecdsa_signature, &len) < 0 || len != 64) {
					return generated_failf(detail, cap,
							"expected 64-byte ECDSA signature");
				}
				if (jsval_subtle_crypto_verify(&region, subtle_a,
						sign_algorithm, ecdsa_public, ecdsa_signature,
						sign_data_buffer, &ecdsa_verify_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_verify(ECDSA)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region,
							ecdsa_verify_promise, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"ecdsa verify drain/result");
				}
				if (result.kind != JSVAL_KIND_BOOL
						|| result.as.boolean != 1) {
					return generated_failf(detail, cap,
							"expected ECDSA verify to resolve true");
				}
			}

			{
				/*
				 * RSASSA-PKCS1-v1_5 transpiled smoke: 2048-bit keygen
				 * resolves to {publicKey, privateKey}, sign a 16-byte
				 * payload with SHA-256, verify it, and confirm the
				 * signature is 256 bytes. publicExponent is implicit F4.
				 */
				jsval_t rsa_usages;
				jsval_t rsa_algorithm;
				jsval_t rsa_name;
				jsval_t rsa_hash_obj;
				jsval_t rsa_hash_name;
				jsval_t rsa_generate_promise;
				jsval_t rsa_keypair;
				jsval_t rsa_public;
				jsval_t rsa_private;
				jsval_t rsa_sign_algorithm;
				jsval_t rsa_data_typed;
				jsval_t rsa_data_buffer;
				jsval_t rsa_sign_promise;
				jsval_t rsa_signature;
				jsval_t rsa_verify_promise;

				if (jsval_array_new(&region, 2, &rsa_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"sign", 4, &result) < 0
						|| jsval_array_push(&region, rsa_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"verify", 6, &result) < 0
						|| jsval_array_push(&region, rsa_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"RSASSA-PKCS1-v1_5", 17,
							&rsa_name) < 0
						|| jsval_object_new(&region, 1, &rsa_hash_obj) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"SHA-256", 7,
							&rsa_hash_name) < 0
						|| jsval_object_set_utf8(&region, rsa_hash_obj,
							(const uint8_t *)"name", 4, rsa_hash_name) < 0
						|| jsval_object_new(&region, 3, &rsa_algorithm) < 0
						|| jsval_object_set_utf8(&region, rsa_algorithm,
							(const uint8_t *)"name", 4, rsa_name) < 0
						|| jsval_object_set_utf8(&region, rsa_algorithm,
							(const uint8_t *)"modulusLength", 13,
							jsval_number(2048.0)) < 0
						|| jsval_object_set_utf8(&region, rsa_algorithm,
							(const uint8_t *)"hash", 4, rsa_hash_obj) < 0) {
					return generated_fail_errno(detail, cap, "rsa setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						rsa_algorithm, 1, rsa_usages,
						&rsa_generate_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(RSA)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region,
							rsa_generate_promise, &rsa_keypair) < 0) {
					return generated_fail_errno(detail, cap,
							"rsa generate drain/result");
				}
				if (rsa_keypair.kind != JSVAL_KIND_OBJECT
						|| jsval_object_get_utf8(&region, rsa_keypair,
							(const uint8_t *)"publicKey", 9,
							&rsa_public) < 0
						|| jsval_object_get_utf8(&region, rsa_keypair,
							(const uint8_t *)"privateKey", 10,
							&rsa_private) < 0
						|| rsa_public.kind != JSVAL_KIND_CRYPTO_KEY
						|| rsa_private.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected RSA generateKey pair");
				}
				if (jsval_object_new(&region, 1, &rsa_sign_algorithm) < 0
						|| jsval_object_set_utf8(&region, rsa_sign_algorithm,
							(const uint8_t *)"name", 4, rsa_name) < 0) {
					return generated_fail_errno(detail, cap,
							"rsa sign alg setup");
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
						16, &rsa_data_typed) < 0
						|| jsval_typed_array_buffer(&region, rsa_data_typed,
							&rsa_data_buffer) < 0) {
					return generated_fail_errno(detail, cap,
							"rsa data buffer");
				}
				if (jsval_subtle_crypto_sign(&region, subtle_a,
						rsa_sign_algorithm, rsa_private, rsa_data_buffer,
						&rsa_sign_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_sign(RSA)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, rsa_sign_promise,
							&rsa_signature) < 0) {
					return generated_fail_errno(detail, cap,
							"rsa sign drain/result");
				}
				if (rsa_signature.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region,
							rsa_signature, &len) < 0 || len != 256) {
					return generated_failf(detail, cap,
							"expected 256-byte RSA-2048 signature");
				}
				if (jsval_subtle_crypto_verify(&region, subtle_a,
						rsa_sign_algorithm, rsa_public, rsa_signature,
						rsa_data_buffer, &rsa_verify_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_verify(RSA)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, rsa_verify_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"rsa verify drain/result");
				}
				if (result.kind != JSVAL_KIND_BOOL
						|| result.as.boolean != 1) {
					return generated_failf(detail, cap,
							"expected RSA verify to resolve true");
				}
			}

			{
				/*
				 * RSA-PSS transpiled smoke: 2048-bit generateKey +
				 * sign + verify with SHA-256, default salt length.
				 * publicExponent implicit F4.
				 */
				jsval_t pss_usages;
				jsval_t pss_algorithm;
				jsval_t pss_name;
				jsval_t pss_hash_obj;
				jsval_t pss_hash_name;
				jsval_t pss_generate_promise;
				jsval_t pss_keypair;
				jsval_t pss_public;
				jsval_t pss_private;
				jsval_t pss_sign_algorithm;
				jsval_t pss_data_typed;
				jsval_t pss_data_buffer;
				jsval_t pss_sign_promise;
				jsval_t pss_signature;
				jsval_t pss_verify_promise;

				if (jsval_array_new(&region, 2, &pss_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"sign", 4, &result) < 0
						|| jsval_array_push(&region, pss_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"verify", 6, &result) < 0
						|| jsval_array_push(&region, pss_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"RSA-PSS", 7, &pss_name) < 0
						|| jsval_object_new(&region, 1, &pss_hash_obj) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"SHA-256", 7,
							&pss_hash_name) < 0
						|| jsval_object_set_utf8(&region, pss_hash_obj,
							(const uint8_t *)"name", 4, pss_hash_name) < 0
						|| jsval_object_new(&region, 3, &pss_algorithm) < 0
						|| jsval_object_set_utf8(&region, pss_algorithm,
							(const uint8_t *)"name", 4, pss_name) < 0
						|| jsval_object_set_utf8(&region, pss_algorithm,
							(const uint8_t *)"modulusLength", 13,
							jsval_number(2048.0)) < 0
						|| jsval_object_set_utf8(&region, pss_algorithm,
							(const uint8_t *)"hash", 4, pss_hash_obj) < 0) {
					return generated_fail_errno(detail, cap, "pss setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						pss_algorithm, 1, pss_usages,
						&pss_generate_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_generate_key(PSS)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region,
							pss_generate_promise, &pss_keypair) < 0) {
					return generated_fail_errno(detail, cap,
							"pss generate drain/result");
				}
				if (pss_keypair.kind != JSVAL_KIND_OBJECT
						|| jsval_object_get_utf8(&region, pss_keypair,
							(const uint8_t *)"publicKey", 9,
							&pss_public) < 0
						|| jsval_object_get_utf8(&region, pss_keypair,
							(const uint8_t *)"privateKey", 10,
							&pss_private) < 0
						|| pss_public.kind != JSVAL_KIND_CRYPTO_KEY
						|| pss_private.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_failf(detail, cap,
							"expected PSS generateKey pair");
				}
				if (jsval_object_new(&region, 1, &pss_sign_algorithm) < 0
						|| jsval_object_set_utf8(&region, pss_sign_algorithm,
							(const uint8_t *)"name", 4, pss_name) < 0) {
					return generated_fail_errno(detail, cap,
							"pss sign alg setup");
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
						16, &pss_data_typed) < 0
						|| jsval_typed_array_buffer(&region, pss_data_typed,
							&pss_data_buffer) < 0) {
					return generated_fail_errno(detail, cap,
							"pss data buffer");
				}
				if (jsval_subtle_crypto_sign(&region, subtle_a,
						pss_sign_algorithm, pss_private, pss_data_buffer,
						&pss_sign_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_sign(PSS)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, pss_sign_promise,
							&pss_signature) < 0) {
					return generated_fail_errno(detail, cap,
							"pss sign drain/result");
				}
				if (pss_signature.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region,
							pss_signature, &len) < 0 || len != 256) {
					return generated_failf(detail, cap,
							"expected 256-byte RSA-PSS signature");
				}
				if (jsval_subtle_crypto_verify(&region, subtle_a,
						pss_sign_algorithm, pss_public, pss_signature,
						pss_data_buffer, &pss_verify_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_verify(PSS)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, pss_verify_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"pss verify drain/result");
				}
				if (result.kind != JSVAL_KIND_BOOL
						|| result.as.boolean != 1) {
					return generated_failf(detail, cap,
							"expected RSA-PSS verify to resolve true");
				}
			}

			{
				/*
				 * SPKI format transpiled smoke. Generate an ECDSA
				 * P-256 keypair, export public as SPKI, reimport,
				 * sign/verify round-trip with the reimported public
				 * key. This exercises the SPKI import + export paths
				 * end-to-end.
				 */
				jsval_t spki_usages;
				jsval_t spki_verify_usages;
				jsval_t spki_ecdsa_alg;
				jsval_t spki_name;
				jsval_t spki_curve;
				jsval_t spki_format_value;
				jsval_t spki_gen_promise;
				jsval_t spki_pair;
				jsval_t spki_public;
				jsval_t spki_private;
				jsval_t spki_export_promise;
				jsval_t spki_bytes;
				jsval_t spki_reimport_promise;
				jsval_t spki_reimported;
				jsval_t spki_sign_alg;
				jsval_t spki_hash_name;
				jsval_t spki_data_typed;
				jsval_t spki_data_buffer;
				jsval_t spki_sign_promise;
				jsval_t spki_signature;
				jsval_t spki_verify_promise;

				if (jsval_array_new(&region, 2, &spki_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"sign", 4, &result) < 0
						|| jsval_array_push(&region, spki_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"verify", 6, &result) < 0
						|| jsval_array_push(&region, spki_usages, result) < 0
						|| jsval_array_new(&region, 1,
							&spki_verify_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"verify", 6, &result) < 0
						|| jsval_array_push(&region, spki_verify_usages,
							result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"ECDSA", 5, &spki_name) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"P-256", 5, &spki_curve) < 0
						|| jsval_object_new(&region, 2, &spki_ecdsa_alg) < 0
						|| jsval_object_set_utf8(&region, spki_ecdsa_alg,
							(const uint8_t *)"name", 4, spki_name) < 0
						|| jsval_object_set_utf8(&region, spki_ecdsa_alg,
							(const uint8_t *)"namedCurve", 10,
							spki_curve) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"spki", 4,
							&spki_format_value) < 0) {
					return generated_fail_errno(detail, cap, "spki setup");
				}
				if (jsval_subtle_crypto_generate_key(&region, subtle_a,
						spki_ecdsa_alg, 1, spki_usages,
						&spki_gen_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"spki ecdsa generateKey");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, spki_gen_promise,
							&spki_pair) < 0) {
					return generated_fail_errno(detail, cap,
							"spki ecdsa generate drain");
				}
				if (jsval_object_get_utf8(&region, spki_pair,
						(const uint8_t *)"publicKey", 9, &spki_public) < 0
						|| jsval_object_get_utf8(&region, spki_pair,
							(const uint8_t *)"privateKey", 10,
							&spki_private) < 0) {
					return generated_fail_errno(detail, cap,
							"spki pair accessors");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a,
						spki_format_value, spki_public,
						&spki_export_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_export_key(spki)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, spki_export_promise,
							&spki_bytes) < 0
						|| spki_bytes.kind != JSVAL_KIND_ARRAY_BUFFER
						|| jsval_array_buffer_byte_length(&region, spki_bytes,
							&len) < 0 || len < 60 || len > 120) {
					return generated_fail_errno(detail, cap,
							"spki export sanity");
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a,
						spki_format_value, spki_bytes, spki_ecdsa_alg, 1,
						spki_verify_usages, &spki_reimport_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_import_key(spki)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, spki_reimport_promise,
							&spki_reimported) < 0
						|| spki_reimported.kind != JSVAL_KIND_CRYPTO_KEY) {
					return generated_fail_errno(detail, cap,
							"spki reimport");
				}
				if (jsval_object_new(&region, 2, &spki_sign_alg) < 0
						|| jsval_object_set_utf8(&region, spki_sign_alg,
							(const uint8_t *)"name", 4, spki_name) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"SHA-256", 7,
							&spki_hash_name) < 0
						|| jsval_object_set_utf8(&region, spki_sign_alg,
							(const uint8_t *)"hash", 4, spki_hash_name) < 0) {
					return generated_fail_errno(detail, cap,
							"spki sign alg setup");
				}
				if (jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8,
						16, &spki_data_typed) < 0
						|| jsval_typed_array_buffer(&region, spki_data_typed,
							&spki_data_buffer) < 0) {
					return generated_fail_errno(detail, cap,
							"spki data buffer");
				}
				if (jsval_subtle_crypto_sign(&region, subtle_a, spki_sign_alg,
						spki_private, spki_data_buffer,
						&spki_sign_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"spki sign");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, spki_sign_promise,
							&spki_signature) < 0) {
					return generated_fail_errno(detail, cap,
							"spki sign drain");
				}
				if (jsval_subtle_crypto_verify(&region, subtle_a,
						spki_sign_alg, spki_reimported, spki_signature,
						spki_data_buffer, &spki_verify_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"spki verify");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, spki_verify_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"spki verify drain");
				}
				if (result.kind != JSVAL_KIND_BOOL
						|| result.as.boolean != 1) {
					return generated_failf(detail, cap,
							"expected SPKI-reimported verify to resolve true");
				}
			}

			{
				jsval_t pbkdf2_usages;
				jsval_t derive_key_only_usages;
				jsval_t sign_verify_usages;
				jsval_t aes_usages;
				jsval_t pbkdf2_algorithm;
				jsval_t pbkdf2_params;
				jsval_t pbkdf2_invalid_params;
				jsval_t hash_object;
				jsval_t raw_format;
				jsval_t jwk_format;
				jsval_t password_input;
				jsval_t salt_input;
				jsval_t pbkdf2_key_promise;
				jsval_t pbkdf2_key;
				jsval_t pbkdf2_key_derivekey_promise;
				jsval_t pbkdf2_key_derivekey;
				jsval_t derive_bits_promise;
				jsval_t derive_hmac_key_promise;
				jsval_t derive_hmac_key;
				jsval_t derive_aes_key_promise;
				jsval_t derive_aes_key;
				jsval_t export_hmac_raw_promise;
				jsval_t export_aes_raw_promise;
				jsval_t invalid_length_promise;
				jsval_t invalid_usage_promise;
				jsval_t unsupported_target_promise;
				jsval_t invalid_salt_promise;
				jsval_t hmac_algorithm;
				jsval_t aes_algorithm;
				jsval_t unsupported_algorithm;

				if (jsval_array_new(&region, 2, &pbkdf2_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"deriveBits", 10, &result) < 0
						|| jsval_array_push(&region, pbkdf2_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"deriveKey", 9, &result) < 0
						|| jsval_array_push(&region, pbkdf2_usages, result) < 0
						|| jsval_array_new(&region, 1, &derive_key_only_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"deriveKey", 9, &result) < 0
						|| jsval_array_push(&region, derive_key_only_usages, result) < 0
						|| jsval_array_new(&region, 2, &sign_verify_usages) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
							&result) < 0
						|| jsval_array_push(&region, sign_verify_usages, result) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
							&result) < 0
						|| jsval_array_push(&region, sign_verify_usages, result) < 0
						|| jsval_array_new(&region, 2, &aes_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"encrypt", 7, &result) < 0
						|| jsval_array_push(&region, aes_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"decrypt", 7, &result) < 0
						|| jsval_array_push(&region, aes_usages, result) < 0
						|| jsval_object_new(&region, 1, &hash_object) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
							&result) < 0
						|| jsval_object_set_utf8(&region, hash_object,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_new(&region, 1, &pbkdf2_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
							&result) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_new(&region, 4, &pbkdf2_params) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
							&result) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_params,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_params,
							(const uint8_t *)"hash", 4, hash_object) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
							&password_input) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
							&salt_input) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_params,
							(const uint8_t *)"salt", 4, salt_input) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_params,
							(const uint8_t *)"iterations", 10,
							jsval_number(2.0)) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"raw", 3,
							&raw_format) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"jwk", 3,
							&jwk_format) < 0) {
					return generated_fail_errno(detail, cap, "pbkdf2 setup");
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
						password_input, pbkdf2_algorithm, 1, pbkdf2_usages,
						&pbkdf2_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_import_key(pbkdf2 raw)");
				}
				if (jsval_promise_state(&region, pbkdf2_key_promise, &promise_state) < 0
						|| promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected PBKDF2 importKey promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, pbkdf2_key_promise,
							&pbkdf2_key) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 import drain/result");
				}
				if (jsval_crypto_key_algorithm(&region, pbkdf2_key, &result) < 0
						|| result.kind != JSVAL_KIND_OBJECT) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 key algorithm");
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"name", 4, &dom_exception) < 0) {
					return generated_fail_errno(detail, cap, "pbkdf2 algorithm name");
				}
				status = generated_expect_string(&region, dom_exception,
						(const uint8_t *)"PBKDF2", 6, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_crypto_key_usages(&region, pbkdf2_key, &usages) < 0) {
					return generated_fail_errno(detail, cap, "pbkdf2 usages");
				}
				if (usages != (JSVAL_CRYPTO_KEY_USAGE_DERIVE_BITS
							| JSVAL_CRYPTO_KEY_USAGE_DERIVE_KEY)) {
					return generated_failf(detail, cap,
							"unexpected PBKDF2 usages");
				}
				if (jsval_subtle_crypto_derive_bits(&region, subtle_a, pbkdf2_params,
						pbkdf2_key, jsval_number(256.0),
						&derive_bits_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_derive_bits");
				}
				if (jsval_promise_state(&region, derive_bits_promise, &promise_state)
						< 0 || promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected PBKDF2 deriveBits promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, derive_bits_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 deriveBits drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						pbkdf2_zero_derivebits_32,
						sizeof(pbkdf2_zero_derivebits_32), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 2, &hmac_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
							&result) < 0
						|| jsval_object_set_utf8(&region, hmac_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, hmac_algorithm,
							(const uint8_t *)"hash", 4, hash_object) < 0) {
					return generated_fail_errno(detail, cap, "pbkdf2 hmac setup");
				}
				if (jsval_subtle_crypto_derive_key(&region, subtle_a, pbkdf2_params,
						pbkdf2_key, hmac_algorithm, 1, sign_verify_usages,
						&derive_hmac_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_derive_key(hmac)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, derive_hmac_key_promise,
							&derive_hmac_key) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 derive hmac drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						derive_hmac_key, &export_hmac_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 export hmac raw");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, export_hmac_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 export hmac raw drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						pbkdf2_zero_derivekey_hmac_64,
						sizeof(pbkdf2_zero_derivekey_hmac_64), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 2, &aes_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM",
							7, &result) < 0
						|| jsval_object_set_utf8(&region, aes_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, aes_algorithm,
							(const uint8_t *)"length", 6,
							jsval_number(128.0)) < 0) {
					return generated_fail_errno(detail, cap, "pbkdf2 aes setup");
				}
				if (jsval_subtle_crypto_derive_key(&region, subtle_a, pbkdf2_params,
						pbkdf2_key, aes_algorithm, 1, aes_usages,
						&derive_aes_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_derive_key(aes)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, derive_aes_key_promise,
							&derive_aes_key) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 derive aes drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						derive_aes_key, &export_aes_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 export aes raw");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, export_aes_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 export aes raw drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						pbkdf2_zero_derivekey_aes_16,
						sizeof(pbkdf2_zero_derivekey_aes_16), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
						password_input, pbkdf2_algorithm, 1, derive_key_only_usages,
						&pbkdf2_key_derivekey_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 deriveKey-only import");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, pbkdf2_key_derivekey_promise,
							&pbkdf2_key_derivekey) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 deriveKey-only drain/result");
				}
				if (jsval_subtle_crypto_derive_bits(&region, subtle_a, pbkdf2_params,
						pbkdf2_key_derivekey, jsval_number(256.0),
						&invalid_usage_promise) < 0
						|| jsval_promise_result(&region, invalid_usage_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 invalid usage");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"InvalidAccessError", 18, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_derive_bits(&region, subtle_a, pbkdf2_params,
						pbkdf2_key, jsval_number(255.0),
						&invalid_length_promise) < 0
						|| jsval_promise_result(&region, invalid_length_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 invalid length");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"OperationError", 14, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 3, &pbkdf2_invalid_params) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"PBKDF2", 6,
							&result) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_invalid_params,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_invalid_params,
							(const uint8_t *)"hash", 4, hash_object) < 0
						|| jsval_object_set_utf8(&region, pbkdf2_invalid_params,
							(const uint8_t *)"iterations", 10,
							jsval_number(2.0)) < 0
						|| jsval_subtle_crypto_derive_bits(&region, subtle_a,
							pbkdf2_invalid_params, pbkdf2_key, jsval_number(256.0),
							&invalid_salt_promise) < 0
						|| jsval_promise_result(&region, invalid_salt_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 invalid salt");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"TypeError", 9, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 2, &unsupported_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"RSA-OAEP", 8,
							&result) < 0
						|| jsval_object_set_utf8(&region, unsupported_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, unsupported_algorithm,
							(const uint8_t *)"length", 6,
							jsval_number(128.0)) < 0
						|| jsval_subtle_crypto_derive_key(&region, subtle_a,
							pbkdf2_params, pbkdf2_key, unsupported_algorithm, 1,
							aes_usages, &unsupported_target_promise) < 0
						|| jsval_promise_result(&region, unsupported_target_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 unsupported target");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"NotSupportedError", 17, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
						password_input, pbkdf2_algorithm, 1, pbkdf2_usages,
						&unsupported_target_promise) < 0
						|| jsval_promise_result(&region, unsupported_target_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"pbkdf2 unsupported format");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"NotSupportedError", 17, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
			}
			{
				jsval_t hkdf_usages;
				jsval_t derive_key_only_usages;
				jsval_t sign_verify_usages;
				jsval_t aes_usages;
				jsval_t hkdf_algorithm;
				jsval_t hkdf_params;
				jsval_t hkdf_key_promise;
				jsval_t hkdf_key;
				jsval_t hkdf_key_derivekey_promise;
				jsval_t hkdf_key_derivekey;
				jsval_t hkdf_missing_salt;
				jsval_t hkdf_missing_info;
				jsval_t hash_object;
				jsval_t key_input;
				jsval_t salt_input;
				jsval_t info_input;
				jsval_t derive_bits_promise;
				jsval_t derive_hmac_key_promise;
				jsval_t derive_hmac_key;
				jsval_t derive_aes_key_promise;
				jsval_t derive_aes_key;
				jsval_t export_hmac_raw_promise;
				jsval_t export_aes_raw_promise;
				jsval_t unsupported_format_promise;
				jsval_t invalid_length_promise;
				jsval_t invalid_usage_promise;
				jsval_t invalid_salt_promise;
				jsval_t invalid_info_promise;
				jsval_t unsupported_target_promise;
				jsval_t hmac_algorithm;
				jsval_t aes_algorithm;
				jsval_t unsupported_algorithm;

				if (jsval_array_new(&region, 2, &hkdf_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"deriveBits", 10, &result) < 0
						|| jsval_array_push(&region, hkdf_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"deriveKey", 9, &result) < 0
						|| jsval_array_push(&region, hkdf_usages, result) < 0
						|| jsval_array_new(&region, 1, &derive_key_only_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"deriveKey", 9, &result) < 0
						|| jsval_array_push(&region, derive_key_only_usages, result) < 0
						|| jsval_array_new(&region, 2, &sign_verify_usages) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"sign", 4,
							&result) < 0
						|| jsval_array_push(&region, sign_verify_usages, result) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"verify", 6,
							&result) < 0
						|| jsval_array_push(&region, sign_verify_usages, result) < 0
						|| jsval_array_new(&region, 2, &aes_usages) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"encrypt", 7, &result) < 0
						|| jsval_array_push(&region, aes_usages, result) < 0
						|| jsval_string_new_utf8(&region,
							(const uint8_t *)"decrypt", 7, &result) < 0
						|| jsval_array_push(&region, aes_usages, result) < 0
						|| jsval_object_new(&region, 1, &hash_object) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"SHA-256", 7,
							&result) < 0
						|| jsval_object_set_utf8(&region, hash_object,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_new(&region, 1, &hkdf_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
							&result) < 0
						|| jsval_object_set_utf8(&region, hkdf_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_new(&region, 4, &hkdf_params) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
							&result) < 0
						|| jsval_object_set_utf8(&region, hkdf_params,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, hkdf_params,
							(const uint8_t *)"hash", 4, hash_object) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
							&key_input) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
							&salt_input) < 0
						|| jsval_typed_array_new(&region, JSVAL_TYPED_ARRAY_UINT8, 8,
							&info_input) < 0
						|| jsval_object_set_utf8(&region, hkdf_params,
							(const uint8_t *)"salt", 4, salt_input) < 0
						|| jsval_object_set_utf8(&region, hkdf_params,
							(const uint8_t *)"info", 4, info_input) < 0) {
					return generated_fail_errno(detail, cap, "hkdf setup");
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
						key_input, hkdf_algorithm, 1, hkdf_usages,
						&hkdf_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_import_key(hkdf raw)");
				}
				if (jsval_promise_state(&region, hkdf_key_promise, &promise_state) < 0
						|| promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected HKDF importKey promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, hkdf_key_promise,
							&hkdf_key) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf import drain/result");
				}
				if (jsval_crypto_key_algorithm(&region, hkdf_key, &result) < 0
						|| result.kind != JSVAL_KIND_OBJECT) {
					return generated_fail_errno(detail, cap, "hkdf key algorithm");
				}
				if (jsval_object_get_utf8(&region, result,
						(const uint8_t *)"name", 4, &dom_exception) < 0) {
					return generated_fail_errno(detail, cap, "hkdf algorithm name");
				}
				status = generated_expect_string(&region, dom_exception,
						(const uint8_t *)"HKDF", 4, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_derive_bits(&region, subtle_a, hkdf_params,
						hkdf_key, jsval_number(256.0), &derive_bits_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_derive_bits(hkdf)");
				}
				if (jsval_promise_state(&region, derive_bits_promise, &promise_state)
						< 0 || promise_state != JSVAL_PROMISE_STATE_PENDING) {
					return generated_failf(detail, cap,
							"expected HKDF deriveBits promise to stay pending");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, derive_bits_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf deriveBits drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						hkdf_zero_derivebits_32,
						sizeof(hkdf_zero_derivebits_32), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 2, &hmac_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"HMAC", 4,
							&result) < 0
						|| jsval_object_set_utf8(&region, hmac_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, hmac_algorithm,
							(const uint8_t *)"hash", 4, hash_object) < 0) {
					return generated_fail_errno(detail, cap, "hkdf hmac setup");
				}
				if (jsval_subtle_crypto_derive_key(&region, subtle_a, hkdf_params,
						hkdf_key, hmac_algorithm, 1, sign_verify_usages,
						&derive_hmac_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_derive_key(hkdf hmac)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, derive_hmac_key_promise,
							&derive_hmac_key) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf derive hmac drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						derive_hmac_key, &export_hmac_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf export hmac raw");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, export_hmac_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf export hmac raw drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						hkdf_zero_derivekey_hmac_64,
						sizeof(hkdf_zero_derivekey_hmac_64), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 2, &aes_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"AES-GCM",
							7, &result) < 0
						|| jsval_object_set_utf8(&region, aes_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, aes_algorithm,
							(const uint8_t *)"length", 6,
							jsval_number(128.0)) < 0) {
					return generated_fail_errno(detail, cap, "hkdf aes setup");
				}
				if (jsval_subtle_crypto_derive_key(&region, subtle_a, hkdf_params,
						hkdf_key, aes_algorithm, 1, aes_usages,
						&derive_aes_key_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"jsval_subtle_crypto_derive_key(hkdf aes)");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, derive_aes_key_promise,
							&derive_aes_key) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf derive aes drain/result");
				}
				if (jsval_subtle_crypto_export_key(&region, subtle_a, raw_format,
						derive_aes_key, &export_aes_raw_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf export aes raw");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, export_aes_raw_promise,
							&result) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf export aes raw drain/result");
				}
				status = generated_expect_array_buffer_bytes(&region, result,
						hkdf_zero_derivekey_aes_16,
						sizeof(hkdf_zero_derivekey_aes_16), detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, raw_format,
						key_input, hkdf_algorithm, 1, derive_key_only_usages,
						&hkdf_key_derivekey_promise) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf deriveKey-only import");
				}
				memset(&error, 0, sizeof(error));
				if (jsval_microtask_drain(&region, &error) < 0
						|| jsval_promise_result(&region, hkdf_key_derivekey_promise,
							&hkdf_key_derivekey) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf deriveKey-only drain/result");
				}
				if (jsval_subtle_crypto_derive_bits(&region, subtle_a, hkdf_params,
						hkdf_key_derivekey, jsval_number(256.0),
						&invalid_usage_promise) < 0
						|| jsval_promise_result(&region, invalid_usage_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap, "hkdf invalid usage");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"InvalidAccessError", 18, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_derive_bits(&region, subtle_a, hkdf_params,
						hkdf_key, jsval_number(255.0), &invalid_length_promise) < 0
						|| jsval_promise_result(&region, invalid_length_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap, "hkdf invalid length");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"OperationError", 14, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 3, &hkdf_missing_salt) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
							&result) < 0
						|| jsval_object_set_utf8(&region, hkdf_missing_salt,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, hkdf_missing_salt,
							(const uint8_t *)"hash", 4, hash_object) < 0
						|| jsval_object_set_utf8(&region, hkdf_missing_salt,
							(const uint8_t *)"info", 4, info_input) < 0
						|| jsval_subtle_crypto_derive_bits(&region, subtle_a,
							hkdf_missing_salt, hkdf_key, jsval_number(256.0),
							&invalid_salt_promise) < 0
						|| jsval_promise_result(&region, invalid_salt_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap, "hkdf invalid salt");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"TypeError", 9, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 3, &hkdf_missing_info) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"HKDF", 4,
							&result) < 0
						|| jsval_object_set_utf8(&region, hkdf_missing_info,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, hkdf_missing_info,
							(const uint8_t *)"hash", 4, hash_object) < 0
						|| jsval_object_set_utf8(&region, hkdf_missing_info,
							(const uint8_t *)"salt", 4, salt_input) < 0
						|| jsval_subtle_crypto_derive_bits(&region, subtle_a,
							hkdf_missing_info, hkdf_key, jsval_number(256.0),
							&invalid_info_promise) < 0
						|| jsval_promise_result(&region, invalid_info_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap, "hkdf invalid info");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"TypeError", 9, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_object_new(&region, 2, &unsupported_algorithm) < 0
						|| jsval_string_new_utf8(&region, (const uint8_t *)"RSA-OAEP", 8,
							&result) < 0
						|| jsval_object_set_utf8(&region, unsupported_algorithm,
							(const uint8_t *)"name", 4, result) < 0
						|| jsval_object_set_utf8(&region, unsupported_algorithm,
							(const uint8_t *)"length", 6,
							jsval_number(128.0)) < 0
						|| jsval_subtle_crypto_derive_key(&region, subtle_a,
							hkdf_params, hkdf_key, unsupported_algorithm, 1,
							aes_usages, &unsupported_target_promise) < 0
						|| jsval_promise_result(&region, unsupported_target_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf unsupported target");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"NotSupportedError", 17, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
				if (jsval_subtle_crypto_import_key(&region, subtle_a, jwk_format,
						key_input, hkdf_algorithm, 1, hkdf_usages,
						&unsupported_format_promise) < 0
						|| jsval_promise_result(&region, unsupported_format_promise,
							&dom_exception) < 0
						|| jsval_dom_exception_name(&region, dom_exception, &result) < 0) {
					return generated_fail_errno(detail, cap,
							"hkdf unsupported format");
				}
				status = generated_expect_string(&region, result,
						(const uint8_t *)"NotSupportedError", 17, detail, cap);
				if (status != GENERATED_PASS) {
					return status;
				}
			}
		}
	#else
		memset(&error, 0, sizeof(error));
		errno = 0;
	if (jsval_crypto_get_random_values(&region, crypto_value, typed_u8,
			&result, &error) >= 0 || errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected crypto backend to be disabled");
	}
	errno = 0;
		if (jsval_subtle_crypto_digest(&region, subtle_a, algorithm_name,
				digest_input, &digest_promise) >= 0 || errno != ENOTSUP) {
			return generated_failf(detail, cap,
					"expected subtle.digest to stay disabled");
		}
		errno = 0;
		if (jsval_subtle_crypto_encrypt(&region, subtle_a, algorithm_name,
				jsval_number(1.0), digest_input, &digest_promise) >= 0
				|| errno != ENOTSUP) {
			return generated_failf(detail, cap,
					"expected subtle.encrypt to stay disabled");
		}
		errno = 0;
		if (jsval_subtle_crypto_decrypt(&region, subtle_a, algorithm_name,
				jsval_number(1.0), digest_input, &digest_promise) >= 0
				|| errno != ENOTSUP) {
			return generated_failf(detail, cap,
					"expected subtle.decrypt to stay disabled");
		}
	#endif
	if (jsval_crypto_key_new(&region, JSVAL_CRYPTO_KEY_TYPE_SECRET, 1,
			algorithm_name, JSVAL_CRYPTO_KEY_USAGE_SIGN
			| JSVAL_CRYPTO_KEY_USAGE_VERIFY, &key_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_crypto_key_new");
	}
	if (jsval_crypto_key_type(&region, key_value, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_crypto_key_type");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"secret", 6,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_crypto_key_extractable(&region, key_value, &extractable) < 0
			|| !extractable) {
		return generated_fail_errno(detail, cap,
				"jsval_crypto_key_extractable");
	}
	if (jsval_crypto_key_usages(&region, key_value, &usages) < 0
			|| usages != (JSVAL_CRYPTO_KEY_USAGE_SIGN
				| JSVAL_CRYPTO_KEY_USAGE_VERIFY)) {
		return generated_fail_errno(detail, cap, "jsval_crypto_key_usages");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"QuotaExceededError",
			sizeof("QuotaExceededError") - 1, &result) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"too many bytes",
				sizeof("too many bytes") - 1, &algorithm_name) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(domexception)");
	}
	if (jsval_dom_exception_new(&region, result, algorithm_name,
			&dom_exception) < 0) {
		return generated_fail_errno(detail, cap, "jsval_dom_exception_new");
	}
	return GENERATED_PASS;
}

static generated_status_t
generated_smoke_jsval_promise(char *detail, size_t cap)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_scheduler_t scheduler;
	generated_scheduler_state_t scheduler_state;
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
	generated_status_t status;

	memset(&generated_promise_identity_state, 0,
			sizeof(generated_promise_identity_state));
	memset(&generated_promise_return_state, 0,
			sizeof(generated_promise_return_state));
	memset(&generated_promise_throw_state, 0,
			sizeof(generated_promise_throw_state));
	memset(&generated_microtask_state, 0, sizeof(generated_microtask_state));
	memset(&scheduler_state, 0, sizeof(scheduler_state));

	jsval_region_init(&region, storage, sizeof(storage));
	memset(&scheduler, 0, sizeof(scheduler));
	scheduler.ctx = &scheduler_state;
	scheduler.on_enqueue = generated_scheduler_on_enqueue;
	scheduler.on_wake = generated_scheduler_on_wake;
	jsval_region_set_scheduler(&region, &scheduler);

	if (jsval_function_new(&region, generated_promise_identity_function, 1, 0,
			jsval_undefined(), &identity_fn) < 0
			|| jsval_function_new(&region, generated_promise_return_function, 0,
				0, jsval_undefined(), &return_fn) < 0
			|| jsval_function_new(&region, generated_promise_throw_function, 1, 0,
				jsval_undefined(), &throw_fn) < 0
			|| jsval_function_new(&region, generated_microtask_function, 0, 0,
				jsval_undefined(), &microtask_fn) < 0) {
		return generated_fail_errno(detail, cap, "jsval_function_new(promise)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"boom", 4, &boom) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"caught", 6,
				&caught_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(promise strings)");
	}

	generated_microtask_state.nested_function = microtask_fn;
	generated_microtask_state.enqueue_nested = 1;
	if (jsval_microtask_enqueue(&region, microtask_fn, 0, NULL) < 0) {
		return generated_fail_errno(detail, cap, "jsval_microtask_enqueue");
	}
	if (jsval_microtask_pending(&region) != 1
			|| scheduler_state.enqueue_count != 1
			|| scheduler_state.wake_count != 1) {
		return generated_failf(detail, cap,
				"expected first microtask enqueue to notify scheduler once");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap, "jsval_microtask_drain");
	}
	if (generated_microtask_state.call_count != 2
			|| jsval_microtask_pending(&region) != 0
			|| scheduler_state.enqueue_count != 2
			|| scheduler_state.wake_count != 1) {
		return generated_failf(detail, cap,
				"expected nested microtask drain to run to completion");
	}

	memset(&scheduler_state, 0, sizeof(scheduler_state));
	if (jsval_promise_new(&region, &promise) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_new");
	}
	if (!jsval_truthy(&region, promise)) {
		return generated_failf(detail, cap, "expected promise to be truthy");
	}
	if (jsval_typeof(&region, promise, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(promise)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"object", 6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promise_state(&region, promise, &state) < 0
			|| state != JSVAL_PROMISE_STATE_PENDING) {
		return generated_fail_errno(detail, cap, "jsval_promise_state(pending)");
	}
	if (jsval_promise_result(&region, promise, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_result(pending)");
	}
	if (result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected pending promise result to stay undefined");
	}

	if (jsval_promise_then(&region, promise, identity_fn, jsval_undefined(),
			&downstream) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_then");
	}
	if (jsval_promise_resolve(&region, promise, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_resolve");
	}
	if (jsval_promise_state(&region, downstream, &state) < 0
			|| state != JSVAL_PROMISE_STATE_PENDING
			|| jsval_microtask_pending(&region) != 1) {
		return generated_failf(detail, cap,
				"expected downstream promise to remain pending until drain");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(promise then)");
	}
	if (generated_promise_identity_state.call_count != 1) {
		return generated_failf(detail, cap,
				"expected fulfilled handler to run exactly once");
	}
	status = generated_expect_number(&region,
			generated_promise_identity_state.last_arg, 7.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promise_result(&region, downstream, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(downstream)");
	}
	status = generated_expect_number(&region, result, 7.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	memset(&generated_promise_identity_state, 0,
			sizeof(generated_promise_identity_state));
	memset(&scheduler_state, 0, sizeof(scheduler_state));
	if (jsval_promise_then(&region, promise, identity_fn, jsval_undefined(),
			&settled_then) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_then(settled)");
	}
	if (jsval_promise_state(&region, settled_then, &state) < 0
			|| state != JSVAL_PROMISE_STATE_PENDING
			|| jsval_microtask_pending(&region) != 1) {
		return generated_failf(detail, cap,
				"expected settled source handlers to stay async");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(settled then)");
	}
	status = generated_expect_number(&region,
			generated_promise_identity_state.last_arg, 7.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	memset(&generated_promise_return_state, 0,
			sizeof(generated_promise_return_state));
	generated_promise_return_state.return_value = caught_value;
	if (jsval_promise_new(&region, &rejected_source) < 0
			|| jsval_promise_catch(&region, rejected_source, return_fn, &caught)
				< 0) {
		return generated_fail_errno(detail, cap, "promise catch setup");
	}
	if (jsval_promise_reject(&region, rejected_source, boom) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_reject");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(catch)");
	}
	status = generated_expect_string(&region,
			generated_promise_return_state.last_arg,
			(const uint8_t *)"boom", 4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promise_result(&region, caught, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_result(caught)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"caught", 6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	memset(&generated_promise_return_state, 0,
			sizeof(generated_promise_return_state));
	generated_promise_return_state.return_value = jsval_undefined();
	if (jsval_promise_new(&region, &finally_source) < 0
			|| jsval_promise_finally(&region, finally_source, return_fn,
				&finally_chain) < 0) {
		return generated_fail_errno(detail, cap, "promise finally setup");
	}
	if (jsval_promise_resolve(&region, finally_source, jsval_number(42.0)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_resolve(finally)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(finally)");
	}
	if (generated_promise_return_state.call_count != 1
			|| generated_promise_return_state.last_argc != 0) {
		return generated_failf(detail, cap,
				"expected finally handler to run without arguments");
	}
	if (jsval_promise_result(&region, finally_chain, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(finally)");
	}
	status = generated_expect_number(&region, result, 42.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	memset(&generated_promise_return_state, 0,
			sizeof(generated_promise_return_state));
	if (jsval_promise_new(&region, &cleanup) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_new(cleanup)");
	}
	generated_promise_return_state.return_value = cleanup;
	if (jsval_promise_new(&region, &finally_rejected_source) < 0
			|| jsval_promise_finally(&region, finally_rejected_source, return_fn,
				&after_finally) < 0) {
		return generated_fail_errno(detail, cap,
				"promise finally rejected setup");
	}
	if (jsval_promise_reject(&region, finally_rejected_source, boom) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_reject(finally)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(finally pending)");
	}
	if (jsval_promise_state(&region, after_finally, &state) < 0
			|| state != JSVAL_PROMISE_STATE_PENDING) {
		return generated_failf(detail, cap,
				"expected finally chain to wait on returned promise");
	}
	if (jsval_promise_resolve(&region, cleanup, jsval_undefined()) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_resolve(cleanup)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(finally settle)");
	}
	if (jsval_promise_result(&region, after_finally, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(after finally)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"boom", 4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promise_new(&region, &outer) < 0
			|| jsval_promise_new(&region, &inner) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_new(adopt)");
	}
	if (jsval_promise_resolve(&region, outer, inner) < 0
			|| jsval_promise_resolve(&region, inner, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "promise adoption resolve");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(adopt)");
	}
	if (jsval_promise_result(&region, outer, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_result(outer)");
	}
	status = generated_expect_number(&region, result, 9.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	memset(&generated_promise_throw_state, 0,
			sizeof(generated_promise_throw_state));
	if (jsval_promise_new(&region, &throwing_source) < 0
			|| jsval_promise_then(&region, throwing_source, throw_fn,
				jsval_undefined(), &rejected_chain) < 0) {
		return generated_fail_errno(detail, cap, "promise throw chain setup");
	}
	if (jsval_promise_resolve(&region, throwing_source, jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_resolve(throw source)");
	}
	memset(&error, 0, sizeof(error));
	if (jsval_microtask_drain(&region, &error) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_microtask_drain(throw chain)");
	}
	if (generated_promise_throw_state.call_count != 1) {
		return generated_failf(detail, cap,
				"expected throwing handler to run exactly once");
	}
	if (jsval_promise_result(&region, rejected_chain, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(rejected chain)");
	}
	if (result.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_failf(detail, cap,
				"expected thrown handler to reject with DOMException");
	}
	if (jsval_dom_exception_name(&region, result, &boom) < 0) {
		return generated_fail_errno(detail, cap, "jsval_dom_exception_name");
	}
	status = generated_expect_string(&region, boom, (const uint8_t *)"Error", 5,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promise_new(&region, &self_promise) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_new(self resolution)");
	}
	if (jsval_promise_resolve(&region, self_promise, self_promise) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_resolve(self)");
	}
	if (jsval_promise_result(&region, self_promise, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promise_result(self)");
	}
	if (result.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_failf(detail, cap,
				"expected self resolution to reject with DOMException");
	}
	if (jsval_dom_exception_name(&region, result, &boom) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_dom_exception_name(self)");
	}
	status = generated_expect_string(&region, boom,
			(const uint8_t *)"TypeError", 9, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promise_new(&region, &fixed) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_new(fixed)");
	}
	if (jsval_promise_resolve(&region, fixed, jsval_number(3.0)) < 0
			|| jsval_promise_reject(&region, fixed, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "promise double settle");
	}
	if (jsval_promise_result(&region, fixed, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_promise_result(fixed)");
	}
	return generated_expect_number(&region, result, 3.0, detail, cap);
}

static generated_status_t generated_smoke_jsval_url_core(char *detail,
		size_t cap)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t input;
	jsval_t url;
	jsval_t idna_url;
	jsval_t wire_url;
	jsval_t params_a;
	jsval_t params_b;
	jsval_t result;
	jsval_t detached;
	jsval_t values;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"https://example.com/base?x=1#old",
			sizeof("https://example.com/base?x=1#old") - 1,
			&input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(url input)");
	}
	if (jsval_url_new(&region, input, 0, jsval_undefined(), &url) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_new");
	}
	if (url.kind != JSVAL_KIND_URL) {
		return generated_failf(detail, cap, "expected URL result kind");
	}
	if (!jsval_truthy(&region, url)) {
		return generated_failf(detail, cap, "expected URL to be truthy");
	}
	if (jsval_typeof(&region, url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_typeof(url)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"object", 6, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"https://ma\xc3\xb1""ana\xe3\x80\x82""example/a?x=1#old",
			sizeof("https://ma\xc3\xb1""ana\xe3\x80\x82""example/a?x=1#old") - 1,
			&input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(idna input)");
	}
	if (jsval_url_new(&region, input, 0, jsval_undefined(), &idna_url) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_new(idna)");
	}
	if (jsval_url_hostname(&region, idna_url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_hostname(idna)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"xn--maana-pta.example",
			sizeof("xn--maana-pta.example") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_url_hostname_display(&region, idna_url, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_hostname_display(idna)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"ma\xc3\xb1""ana.example",
			sizeof("ma\xc3\xb1""ana.example") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_url_origin_display(&region, idna_url, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_origin_display(idna)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"https://ma\xc3\xb1""ana.example",
			sizeof("https://ma\xc3\xb1""ana.example") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"https://xn--maana-pta.example/a?x=1#old",
			sizeof("https://xn--maana-pta.example/a?x=1#old") - 1,
			&input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(ace input)");
	}
	if (jsval_url_new(&region, input, 0, jsval_undefined(), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_new(ace)");
	}
	if (result.kind != JSVAL_KIND_URL) {
		return generated_failf(detail, cap, "expected ACE URL result kind");
	}
	if (jsval_url_hostname_display(&region, result, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_hostname_display(ace)");
	}
	status = generated_expect_string(&region, input,
			(const uint8_t *)"ma\xc3\xb1""ana.example",
			sizeof("ma\xc3\xb1""ana.example") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"b\xc3\xbc""cher\xef\xbc\x8e""example:8443",
			sizeof("b\xc3\xbc""cher\xef\xbc\x8e""example:8443") - 1, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(idna host)");
	}
	if (jsval_url_set_host(&region, idna_url, input) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_set_host(idna)");
	}
	if (jsval_url_href(&region, idna_url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_href(idna)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"https://xn--bcher-kva.example:8443/a?x=1#old",
			sizeof("https://xn--bcher-kva.example:8443/a?x=1#old") - 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_url_host_display(&region, idna_url, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_host_display(idna)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"b\xc3\xbc""cher.example:8443",
			sizeof("b\xc3\xbc""cher.example:8443") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"https://xn--.example/a",
			sizeof("https://xn--.example/a") - 1, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(invalid ace)");
	}
	errno = 0;
	if (jsval_url_new(&region, input, 0, jsval_undefined(), &result) >= 0
			|| errno != EINVAL) {
		return generated_failf(detail, cap,
				"expected invalid ACE hostname rejection");
	}
	if (jsval_url_search_params(&region, url, &params_a) < 0
			|| jsval_url_search_params(&region, url, &params_b) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_search_params");
	}
	if (jsval_strict_eq(&region, params_a, params_b) != 1) {
		return generated_failf(detail, cap,
				"expected stable searchParams identity");
	}
	if (!jsval_truthy(&region, params_a)) {
		return generated_failf(detail, cap,
				"expected URLSearchParams to be truthy");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"y", 1, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(name y)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"2", 1, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(value 2)");
	}
	if (jsval_url_search_params_append(&region, params_a, input, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_append(attached)");
	}
	if (jsval_url_href(&region, url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_href");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"https://example.com/base?x=1&y=2#old",
			sizeof("https://example.com/base?x=1&y=2#old") - 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"?a=3", 4, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(search)");
	}
	if (jsval_url_set_search(&region, url, input) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_set_search");
	}
	if (jsval_url_search_params_to_string(&region, params_a, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_to_string(attached)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"a=3", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region,
			(const uint8_t *)
			"https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80",
			sizeof("https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80") - 1,
			&input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(wire input)");
	}
	if (jsval_url_new(&region, input, 0, jsval_undefined(), &wire_url) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_new(wire)");
	}
	if (jsval_url_pathname(&region, wire_url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_pathname(wire)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"/a b/\xf0\x9f\x98\x80",
			sizeof("/a b/\xf0\x9f\x98\x80") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_url_search(&region, wire_url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_search(wire)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"?q=two words&plus=%2B&pair=a%26b%3Dc",
			sizeof("?q=two words&plus=%2B&pair=a%26b%3Dc") - 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_url_href(&region, wire_url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_href(wire)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)
			"https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80",
			sizeof("https://example.com/a%20b/%F0%9F%98%80?q=two%20words&plus=%2B&pair=a%26b%3Dc#frag%20%F0%9F%98%80") - 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"?q=hello world&plus=%2B",
			sizeof("?q=hello world&plus=%2B") - 1, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(wire search)");
	}
	if (jsval_url_set_search(&region, wire_url, input) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_set_search(wire)");
	}
	if (jsval_url_search_params(&region, wire_url, &params_b) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params(wire)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"pair", 4, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(pair name)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"a&b=c", 5, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(pair value)");
	}
	if (jsval_url_search_params_append(&region, params_b, input, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_append(wire)");
	}
	if (jsval_url_search(&region, wire_url, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_url_search(sync)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"?q=hello world&plus=%2B&pair=a%26b%3Dc",
			sizeof("?q=hello world&plus=%2B&pair=a%26b%3Dc") - 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_url_search_params_to_string(&region, params_b, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_to_string(wire)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"q=hello+world&plus=%2B&pair=a%26b%3Dc",
			sizeof("q=hello+world&plus=%2B&pair=a%26b%3Dc") - 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"?b=4", 4, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(detached input)");
	}
	if (jsval_url_search_params_new(&region, input, &detached) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_new");
	}
	if (detached.kind != JSVAL_KIND_URL_SEARCH_PARAMS) {
		return generated_failf(detail, cap,
				"expected detached URLSearchParams result kind");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"c", 1, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(name c)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"5", 1, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(value 5)");
	}
	if (jsval_url_search_params_append(&region, detached, input, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_append(detached)");
	}
	if (jsval_url_search_params_to_string(&region, detached, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_to_string(detached)");
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"b=4&c=5", 7, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"c", 1, &input) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(getAll name)");
	}
	if (jsval_url_search_params_get_all(&region, detached, input, &values) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_url_search_params_get_all");
	}
	{
		static const char *expected_values[] = {"5"};

		status = generated_expect_string_array(&region, values,
				expected_values, 1, detail, cap);
		if (status != GENERATED_PASS) {
			return status;
		}
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_nullish_coalescing(char *detail,
		size_t cap)
{
	static const uint8_t json[] =
		"{\"nothing\":null,\"flag\":false,\"zero\":0,\"empty\":\"\",\"text\":\"x\",\"obj\":{}}";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t root;
	jsval_t nothing;
	jsval_t flag;
	jsval_t zero;
	jsval_t empty;
	jsval_t text;
	jsval_t obj;
	jsval_t other_obj;
	jsval_t fallback;
	jsval_t left;
	jsval_t result;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_is_nullish(jsval_undefined()) != 1
			|| jsval_is_nullish(jsval_null()) != 1
			|| jsval_is_nullish(jsval_bool(0)) != 0
			|| jsval_is_nullish(jsval_number(0.0)) != 0
			|| jsval_is_nullish(jsval_number(NAN)) != 0) {
		return generated_failf(detail, cap,
				"unexpected primitive jsval_is_nullish result");
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"fallback", 8,
			&fallback) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(fallback)");
	}
	if (jsval_object_new(&region, 0, &other_obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new");
	}

	if (jsval_json_parse(&region, json, sizeof(json) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(nothing)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(flag)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"zero", 4,
			&zero) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(zero)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"empty", 5,
			&empty) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(empty)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(obj)");
	}

	if (jsval_is_nullish(nothing) != 1 || jsval_is_nullish(flag) != 0
			|| jsval_is_nullish(zero) != 0 || jsval_is_nullish(empty) != 0
			|| jsval_is_nullish(text) != 0 || jsval_is_nullish(obj) != 0) {
		return generated_failf(detail, cap,
				"unexpected parsed jsval_is_nullish result");
	}

	left = jsval_undefined();
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"fallback", 8, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	left = nothing;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"fallback", 8, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	left = flag;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	if (jsval_strict_eq(&region, result, jsval_bool(0)) != 1) {
		return generated_failf(detail, cap,
				"expected false to survive nullish coalescing");
	}

	left = zero;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	if (jsval_strict_eq(&region, result, jsval_number(0.0)) != 1) {
		return generated_failf(detail, cap,
				"expected 0 to survive nullish coalescing");
	}

	left = empty;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"", 0,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	left = text;
	if (jsval_is_nullish(left)) {
		result = fallback;
	} else {
		result = left;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"x", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	left = obj;
	if (jsval_is_nullish(left)) {
		result = other_obj;
	} else {
		result = left;
	}
	if (jsval_strict_eq(&region, result, obj) != 1) {
		return generated_failf(detail, cap,
				"expected object identity to survive nullish coalescing");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_ternary(char *detail,
		size_t cap)
{
	static const uint8_t json[] =
		"{\"flag\":false,\"zero\":0,\"empty\":\"\",\"text\":\"x\",\"obj\":{},\"arr\":[]}";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t root;
	jsval_t flag;
	jsval_t zero;
	jsval_t empty;
	jsval_t text;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_text;
	jsval_t native_obj;
	jsval_t native_arr;
	jsval_t other_obj;
	jsval_t other_arr;
	jsval_t then_value;
	jsval_t else_value;
	jsval_t result;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&native_text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(x)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"then", 4,
			&then_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(then)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"else", 4,
			&else_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(else)");
	}
	if (jsval_object_new(&region, 0, &native_obj) < 0
			|| jsval_object_new(&region, 0, &other_obj) < 0
			|| jsval_array_new(&region, 0, &native_arr) < 0
			|| jsval_array_new(&region, 0, &other_arr) < 0) {
		return generated_fail_errno(detail, cap, "native container allocation");
	}

	if (jsval_json_parse(&region, json, sizeof(json) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"zero", 4,
				&zero) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"empty", 5,
				&empty) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
				&text) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
				&obj) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
				&arr) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8");
	}

	if (jsval_truthy(&region, jsval_undefined())) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"else",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, jsval_null())) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"else",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, flag)) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"else",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, zero)) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"else",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, empty)) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"else",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, text)) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"then",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, native_text)) {
		result = then_value;
	} else {
		result = else_value;
	}
	status = generated_expect_string(&region, result, (const uint8_t *)"then",
			4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, obj)) {
		result = obj;
	} else {
		result = other_obj;
	}
	if (jsval_strict_eq(&region, result, obj) != 1) {
		return generated_failf(detail, cap,
				"expected parsed object truthiness to select the then branch");
	}

	if (jsval_truthy(&region, native_obj)) {
		result = native_obj;
	} else {
		result = other_obj;
	}
	if (jsval_strict_eq(&region, result, native_obj) != 1) {
		return generated_failf(detail, cap,
				"expected native object truthiness to select the then branch");
	}

	if (jsval_truthy(&region, arr)) {
		result = arr;
	} else {
		result = other_arr;
	}
	if (jsval_strict_eq(&region, result, arr) != 1) {
		return generated_failf(detail, cap,
				"expected parsed array truthiness to select the then branch");
	}

	if (jsval_truthy(&region, native_arr)) {
		result = native_arr;
	} else {
		result = other_arr;
	}
	if (jsval_strict_eq(&region, result, native_arr) != 1) {
		return generated_failf(detail, cap,
				"expected native array truthiness to select the then branch");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_strict_equality(char *detail,
		size_t cap)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t null_word;
	jsval_t one_string;
	jsval_t undefined_word;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"1\")");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"null", 4, &null_word) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"null\")");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"undefined", 9,
			&undefined_word) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(\"undefined\")");
	}

	if (jsval_strict_eq(&region, jsval_number(NAN), jsval_bool(1)) != 0
			|| jsval_strict_eq(&region, jsval_number(NAN), jsval_number(1.0)) != 0
			|| jsval_strict_eq(&region, jsval_number(NAN), jsval_number(NAN)) != 0
			|| jsval_strict_eq(&region, jsval_number(NAN), jsval_number(INFINITY)) != 0) {
		return generated_failf(detail, cap,
				"expected NaN strict-equality checks to stay false");
	}
	if (jsval_strict_eq(&region, jsval_number(+0.0), jsval_number(-0.0)) != 1
			|| jsval_strict_eq(&region, jsval_number(-0.0), jsval_number(+0.0)) != 1) {
		return generated_failf(detail, cap,
				"expected +0 and -0 to stay strictly equal");
	}
	if (jsval_strict_eq(&region, jsval_number(INFINITY), jsval_number(INFINITY)) != 1
			|| jsval_strict_eq(&region, jsval_number(-INFINITY), jsval_number(-INFINITY)) != 1
			|| jsval_strict_eq(&region, jsval_number(13.0), jsval_number(13.0)) != 1
			|| jsval_strict_eq(&region, jsval_number(1.3), jsval_number(1.3)) != 1
			|| jsval_strict_eq(&region, jsval_number(1.0), jsval_number(0.999999999999)) != 0) {
		return generated_failf(detail, cap,
				"unexpected primitive numeric strict-equality result");
	}
	if (jsval_strict_eq(&region, jsval_number(1.0), jsval_bool(1)) != 0
			|| jsval_strict_eq(&region, jsval_number(1.0), one_string) != 0
			|| jsval_strict_eq(&region, jsval_bool(0), jsval_number(0.0)) != 0) {
		return generated_failf(detail, cap,
				"expected primitive cross-type strict equality to stay false");
	}
	if (jsval_strict_eq(&region, jsval_undefined(), jsval_null()) != 0
			|| jsval_strict_eq(&region, jsval_null(), jsval_undefined()) != 0
			|| jsval_strict_eq(&region, jsval_null(), jsval_number(0.0)) != 0
			|| jsval_strict_eq(&region, jsval_null(), jsval_bool(0)) != 0
			|| jsval_strict_eq(&region, jsval_null(), null_word) != 0
			|| jsval_strict_eq(&region, jsval_undefined(), undefined_word) != 0) {
		return generated_failf(detail, cap,
				"expected null/undefined strict equality to stay type-sensitive");
	}

	if (!(jsval_strict_eq(&region, jsval_undefined(), jsval_null()) == 0)
			|| !(jsval_strict_eq(&region, jsval_number(1.0), one_string) == 0)) {
		return generated_failf(detail, cap,
				"expected explicit strict-inequality mirrors to hold");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_logical_not(char *detail,
		size_t cap)
{
	uint8_t storage[1024];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t object;
	jsval_t prop;
	int c = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"\")");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"1\")");
	}
	if (jsval_object_new(&region, 1, &object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new");
	}
	if (jsval_object_set_utf8(&region, object, (const uint8_t *)"prop", 4,
			jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(prop)");
	}
	if (jsval_object_get_utf8(&region, object, (const uint8_t *)"prop", 4,
			&prop) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(prop)");
	}

	if ((!jsval_truthy(&region, jsval_bool(1))) != 0
			|| (!(!jsval_truthy(&region, jsval_bool(1)))) != 1
			|| (!jsval_truthy(&region, prop)) != 0
			|| (!jsval_truthy(&region, jsval_bool(0))) != 1
			|| (!jsval_truthy(&region, jsval_null())) != 1
			|| (!jsval_truthy(&region, jsval_undefined())) != 1
			|| (!jsval_truthy(&region, jsval_number(+0.0))) != 1
			|| (!jsval_truthy(&region, jsval_number(-0.0))) != 1
			|| (!jsval_truthy(&region, jsval_number(NAN))) != 1
			|| (!jsval_truthy(&region, jsval_number(INFINITY))) != 0
			|| (!jsval_truthy(&region, empty_string)) != 1
			|| (!jsval_truthy(&region, one_string)) != 0) {
		return generated_failf(detail, cap,
				"unexpected logical-not truthiness result");
	}

	if (!jsval_truthy(&region, jsval_number(1.0))) {
		return generated_failf(detail, cap, "expected 1 to be truthy");
	} else {
		c++;
	}
	if (!jsval_truthy(&region, jsval_bool(1))) {
		return generated_failf(detail, cap, "expected true to be truthy");
	} else {
		c++;
	}
	if (!jsval_truthy(&region, one_string)) {
		return generated_failf(detail, cap, "expected \"1\" to be truthy");
	} else {
		c++;
	}
	if (c != 3) {
		return generated_failf(detail, cap,
				"expected truthy if/else counter to reach 3, got %d", c);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_logical_and_or(char *detail,
		size_t cap)
{
	uint8_t storage[1024];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t minus_one_string;
	jsval_t x_string;
	jsval_t result;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, (const uint8_t *)"", 0, &empty_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"\")");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"1\")");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"-1", 2,
			&minus_one_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"-1\")");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"x", 1, &x_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"x\")");
	}

	result = generated_logical_and(&region, jsval_bool(0), jsval_bool(1));
	if (jsval_strict_eq(&region, result, jsval_bool(0)) != 1) {
		return generated_failf(detail, cap,
				"expected false && true to return false");
	}
	result = generated_logical_and(&region, jsval_bool(1), jsval_bool(0));
	if (jsval_strict_eq(&region, result, jsval_bool(0)) != 1) {
		return generated_failf(detail, cap,
				"expected true && false to return false");
	}
	result = generated_logical_and(&region, jsval_number(-0.0), jsval_number(-1.0));
	status = generated_expect_negative_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_and(&region, jsval_number(0.0), jsval_number(-1.0));
	status = generated_expect_positive_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_and(&region, jsval_number(NAN), jsval_number(1.0));
	status = generated_expect_nan_value(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_and(&region, empty_string, one_string);
	status = generated_expect_string(&region, result, (const uint8_t *)"", 0,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_and(&region, one_string, x_string);
	status = generated_expect_string(&region, result, (const uint8_t *)"x", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	result = generated_logical_or(&region, jsval_bool(0), jsval_bool(1));
	if (jsval_strict_eq(&region, result, jsval_bool(1)) != 1) {
		return generated_failf(detail, cap,
				"expected false || true to return true");
	}
	result = generated_logical_or(&region, jsval_number(0.0), jsval_number(-0.0));
	status = generated_expect_negative_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_or(&region, jsval_number(-0.0), jsval_number(0.0));
	status = generated_expect_positive_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_or(&region, empty_string, one_string);
	status = generated_expect_string(&region, result, (const uint8_t *)"1", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_or(&region, jsval_number(-1.0), jsval_number(1.0));
	status = generated_expect_number(&region, result, -1.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	result = generated_logical_or(&region, minus_one_string, x_string);
	status = generated_expect_string(&region, result, (const uint8_t *)"-1", 2,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_numeric_coercion(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"truth\":true,\"nothing\":null,\"one\":\"1\",\"spaces\":\"   \",\"bad\":\"x\",\"inf\":\"Infinity\",\"upper\":\"INFINITY\"}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t nothing;
	jsval_t one;
	jsval_t spaces;
	jsval_t bad;
	jsval_t inf;
	jsval_t upper;
	jsval_t result;
	double number;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
				&nothing) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
				&one) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"spaces", 6,
				&spaces) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
				&bad) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"inf", 3,
				&inf) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"upper", 5,
				&upper) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8");
	}

	if (jsval_to_number(&region, jsval_undefined(), &number) < 0 || !isnan(number)) {
		return generated_failf(detail, cap,
				"expected undefined ToNumber result to be NaN");
	}
	if (jsval_to_number(&region, jsval_null(), &number) < 0 || number != 0.0) {
		return generated_failf(detail, cap,
				"expected null ToNumber result to be +0");
	}
	if (jsval_to_number(&region, truth, &number) < 0 || number != 1.0
			|| jsval_to_number(&region, nothing, &number) < 0 || number != 0.0
			|| jsval_to_number(&region, one, &number) < 0 || number != 1.0
			|| jsval_to_number(&region, spaces, &number) < 0 || number != 0.0) {
		return generated_failf(detail, cap,
				"unexpected primitive ToNumber result");
	}
	if (jsval_to_number(&region, bad, &number) < 0 || !isnan(number)) {
		return generated_failf(detail, cap,
				"expected invalid string ToNumber result to be NaN");
	}
	if (jsval_to_number(&region, inf, &number) < 0
			|| !isinf(number) || number < 0) {
		return generated_failf(detail, cap,
				"expected exact Infinity ToNumber result");
	}
	if (jsval_to_number(&region, upper, &number) < 0 || !isnan(number)) {
		return generated_failf(detail, cap,
				"expected INFINITY ToNumber result to be NaN");
	}

	if (jsval_unary_plus(&region, truth, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_unary_plus(true)");
	}
	status = generated_expect_number(&region, result, 1.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_unary_plus(&region, jsval_null(), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_unary_plus(null)");
	}
	status = generated_expect_positive_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_unary_plus(&region, upper, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_unary_plus(INFINITY)");
	}
	status = generated_expect_nan_value(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_unary_minus(&region, jsval_bool(0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_unary_minus(false)");
	}
	status = generated_expect_negative_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_unary_minus(&region, jsval_number(-0.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_unary_minus(-0)");
	}
	status = generated_expect_positive_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_subtract(&region, jsval_bool(1), jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_subtract(true, 1)");
	}
	status = generated_expect_positive_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_multiply(&region, jsval_bool(1), jsval_bool(1), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_multiply(true, true)");
	}
	status = generated_expect_number(&region, result, 1.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_divide(&region, jsval_bool(1), jsval_bool(1), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_divide(true, true)");
	}
	status = generated_expect_number(&region, result, 1.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_remainder(&region, jsval_bool(1), jsval_bool(1), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_remainder(true, true)");
	}
	status = generated_expect_positive_zero(result, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_bitwise(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"truth\":true,\"zero\":\"0\",\"one\":\"1\",\"bad\":\"x\",\"nothing\":null,\"num\":5,\"obj\":{}}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t zero;
	jsval_t one;
	jsval_t bad;
	jsval_t nothing;
	jsval_t num;
	jsval_t obj;
	jsval_t result;
	int32_t i32;
	uint32_t u32;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"zero", 4,
				&zero) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
				&one) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
				&bad) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
				&nothing) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
				&num) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
				&obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8");
	}

	if (jsval_to_int32(&region, one, &i32) < 0) {
		return generated_fail_errno(detail, cap, "jsval_to_int32(\"1\")");
	}
	if (i32 != 1) {
		return generated_failf(detail, cap, "expected ToInt32(\"1\") to be 1");
	}
	if (jsval_to_uint32(&region, jsval_number(4294967295.0), &u32) < 0) {
		return generated_fail_errno(detail, cap, "jsval_to_uint32(4294967295)");
	}
	if (u32 != 4294967295u) {
		return generated_failf(detail, cap,
				"expected ToUint32(4294967295) to be 4294967295");
	}
	if (jsval_bitwise_not(&region, nothing, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bitwise_not(null)");
	}
	if (generated_expect_number(&region, result, -1.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_bitwise_and(&region, truth, num, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bitwise_and(true, 5)");
	}
	if (generated_expect_number(&region, result, 1.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_bitwise_or(&region, one, jsval_bool(0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bitwise_or(\"1\", false)");
	}
	if (generated_expect_number(&region, result, 1.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_bitwise_xor(&region, bad, jsval_bool(1), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_bitwise_xor(\"x\", true)");
	}
	if (generated_expect_number(&region, result, 1.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	errno = 0;
	if (jsval_bitwise_not(&region, obj, &result) == 0) {
		return generated_failf(detail, cap,
				"object bitwise-not unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP for object bitwise-not, got %d", errno);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_shift(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"truth\":true,\"count\":\"33\",\"neg\":-1,\"num\":5,\"obj\":{}}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t count;
	jsval_t neg;
	jsval_t num;
	jsval_t obj;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"count", 5,
				&count) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"neg", 3,
				&neg) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
				&num) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
				&obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8");
	}

	if (jsval_shift_left(&region, jsval_bool(1), jsval_bool(1), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_shift_left(true, true)");
	}
	if (generated_expect_number(&region, result, 2.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_shift_right(&region, jsval_number(-1.0), jsval_number(1.0),
			&result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_shift_right(-1, 1)");
	}
	if (generated_expect_number(&region, result, -1.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_shift_right_unsigned(&region, jsval_number(-1.0),
			jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_shift_right_unsigned(-1, 1)");
	}
	if (generated_expect_number(&region, result, 2147483647.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_shift_left(&region, truth, count, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_shift_left(parsed truth, parsed count)");
	}
	if (generated_expect_number(&region, result, 2.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_shift_right(&region, num, count, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_shift_right(parsed num, parsed count)");
	}
	if (generated_expect_number(&region, result, 2.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_shift_right_unsigned(&region, neg, truth, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_shift_right_unsigned(parsed neg, parsed truth)");
	}
	if (generated_expect_number(&region, result, 2147483647.0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	errno = 0;
	if (jsval_shift_left(&region, obj, jsval_bool(1), &result) == 0) {
		return generated_failf(detail, cap,
				"object shift unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP for object shift, got %d", errno);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_relational(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"truth\":true,\"nothing\":null,\"one\":\"1\",\"bad\":\"x\",\"num\":2,\"left\":\"1\",\"right\":\"2\"}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t nothing;
	jsval_t one;
	jsval_t bad;
	jsval_t num;
	jsval_t left_string;
	jsval_t right_string;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"truth", 5,
			&truth) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
				&nothing) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"one", 3,
				&one) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"bad", 3,
				&bad) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
				&num) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"left", 4,
				&left_string) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"right", 5,
				&right_string) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8");
	}

	if (jsval_less_than(&region, jsval_bool(1), jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_less_than(true, 1)");
	}
	if (generated_expect_boolean_result(result, 0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_less_equal(&region, jsval_bool(1), jsval_number(1.0), &result)
			< 0) {
		return generated_fail_errno(detail, cap, "jsval_less_equal(true, 1)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_greater_than(&region, num, truth, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_greater_than(num, truth)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_less_than(&region, nothing, truth, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_less_than(null, true)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_less_than(&region, one, truth, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_less_than(\"1\", true)");
	}
	if (generated_expect_boolean_result(result, 0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_greater_equal(&region, truth, one, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_greater_equal(true, \"1\")");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_less_than(&region, bad, jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_less_than(\"x\", 1)");
	}
	if (generated_expect_boolean_result(result, 0, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_less_than(&region, left_string, right_string, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_less_than(\"1\", \"2\")");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_greater_than(&region, right_string, left_string, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_greater_than(\"2\", \"1\")");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_abstract_equality(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"num\":1,\"flag\":true,\"text\":\"1\",\"empty\":\"\",\"nothing\":null,\"obj\":{}}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t text;
	jsval_t empty;
	jsval_t nothing;
	jsval_t obj;
	jsval_t one;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3, &num)
			< 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
				&flag) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
				&text) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"empty", 5,
				&empty) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
				&nothing) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
				&obj) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"1", 1, &one) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8");
	}

	if (jsval_abstract_eq(&region, jsval_bool(1), jsval_number(1.0), &result)
			< 0) {
		return generated_fail_errno(detail, cap, "jsval_abstract_eq(true, 1)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_abstract_eq(&region, num, one, &result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_abstract_eq(num, \"1\")");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_abstract_eq(&region, flag, one, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_eq(flag, \"1\")");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_abstract_eq(&region, text, jsval_number(1.0), &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_eq(text, 1)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_abstract_eq(&region, empty, jsval_number(0.0), &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_eq(empty, 0)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_abstract_eq(&region, nothing, jsval_undefined(), &result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_eq(null, undefined)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_abstract_ne(&region, jsval_number(NAN), jsval_number(NAN),
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_abstract_ne(NaN, NaN)");
	}
	if (generated_expect_boolean_result(result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	errno = 0;
	if (jsval_abstract_eq(&region, obj, obj, &result) == 0) {
		return generated_failf(detail, cap,
				"object abstract equality unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP for object abstract equality, got %d", errno);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_json_value_parity(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"num\":1,\"flag\":true,\"off\":false,\"text\":\"2\",\"nothing\":null,\"obj\":{},\"obj2\":{},\"arr\":[],\"arr2\":[]}";
	uint8_t storage[32768];
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
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 48, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"num", 3,
			&num) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(num)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"flag", 4,
			&flag) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(flag)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"off", 3,
			&off) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(off)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"nothing", 7,
			&nothing) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(nothing)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"obj", 3,
			&obj) < 0 || jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj", 3, &obj_again) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"obj2", 4,
			&obj2) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(obj)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"arr", 3,
			&arr) < 0 || jsval_object_get_utf8(&region, root,
			(const uint8_t *)"arr", 3, &arr_again) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"arr2", 4,
			&arr2) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(arr)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
			&native_text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(\"2\")");
	}
	if (jsval_object_new(&region, 0, &native_obj) < 0
			|| jsval_object_new(&region, 0, &native_obj_other) < 0
			|| jsval_array_new(&region, 0, &native_arr) < 0
			|| jsval_array_new(&region, 0, &native_arr_other) < 0) {
		return generated_fail_errno(detail, cap, "jsval native container allocation");
	}

	if (jsval_truthy(&region, num) != jsval_truthy(&region, jsval_number(1.0))
			|| jsval_truthy(&region, flag) != jsval_truthy(&region, jsval_bool(1))
			|| jsval_truthy(&region, off) != jsval_truthy(&region, jsval_bool(0))
			|| jsval_truthy(&region, text) != jsval_truthy(&region, native_text)
			|| jsval_truthy(&region, nothing) != jsval_truthy(&region, jsval_null())) {
		return generated_failf(detail, cap,
				"expected parsed primitive truthiness to match native values");
	}
	if (jsval_strict_eq(&region, num, jsval_number(1.0)) != 1
			|| jsval_strict_eq(&region, flag, jsval_bool(1)) != 1
			|| jsval_strict_eq(&region, off, jsval_bool(0)) != 1
			|| jsval_strict_eq(&region, text, native_text) != 1
			|| jsval_strict_eq(&region, nothing, jsval_null()) != 1) {
		return generated_failf(detail, cap,
				"expected parsed primitive strict equality to match native values");
	}

	if (jsval_add(&region, text, jsval_number(1.0), &sum) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(text, 1)");
	}
	status = generated_expect_string(&region, sum, (const uint8_t *)"21", 2,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_add(&region, flag, jsval_number(1.0), &sum) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(flag, 1)");
	}
	status = generated_expect_number(&region, sum, 2.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_add(&region, nothing, jsval_number(1.0), &sum) < 0) {
		return generated_fail_errno(detail, cap, "jsval_add(null, 1)");
	}
	status = generated_expect_number(&region, sum, 1.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_truthy(&region, obj) != 1 || jsval_truthy(&region, arr) != 1
			|| jsval_truthy(&region, native_obj) != 1
			|| jsval_truthy(&region, native_arr) != 1) {
		return generated_failf(detail, cap,
				"expected objects and arrays to stay truthy");
	}
	if (jsval_strict_eq(&region, obj, obj_again) != 1
			|| jsval_strict_eq(&region, obj, obj2) != 0
			|| jsval_strict_eq(&region, arr, arr_again) != 1
			|| jsval_strict_eq(&region, arr, arr2) != 0
			|| jsval_strict_eq(&region, native_obj, native_obj) != 1
			|| jsval_strict_eq(&region, native_obj, native_obj_other) != 0
			|| jsval_strict_eq(&region, native_arr, native_arr) != 1
			|| jsval_strict_eq(&region, native_arr, native_arr_other) != 0) {
		return generated_failf(detail, cap,
				"unexpected container identity result");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_shallow_planned_promotion(
		char *detail, size_t cap)
{
	static const uint8_t input[] =
		"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2],\"status\":\"ok\"}";
	static const uint8_t expected_json[] =
		"{\"profile\":{\"name\":\"Ada\"},\"scores\":[1,2,3,4],\"status\":\"ok\",\"ready\":true}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t profile;
	jsval_t scores;
	jsval_t got;
	size_t bytes;
	size_t before_used;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7,
			&profile) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(profile)");
	}
	if (!jsval_is_json_backed(profile)) {
		return generated_failf(detail, cap,
				"expected parsed child object to remain JSON-backed");
	}

	before_used = region.used;
	if (jsval_promote_object_shallow_measure(&region, root, 4, &bytes) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_measure(root)");
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 4) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(root)");
	}
	if (!jsval_is_native(root)) {
		return generated_failf(detail, cap,
				"expected shallow-promoted root to be native");
	}
	if (region.used != before_used + bytes) {
		return generated_failf(detail, cap,
				"unexpected byte count from shallow object promotion");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"profile", 7,
			&profile) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(profile after root promote)");
	}
	if (!jsval_is_json_backed(profile)) {
		return generated_failf(detail, cap,
				"expected shallow-promoted root to keep child object JSON-backed");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"ready", 5,
			jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(ready)");
	}

	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"scores", 6,
			&scores) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(scores)");
	}
	if (!jsval_is_json_backed(scores)) {
		return generated_failf(detail, cap,
				"expected child array to stay JSON-backed until explicitly promoted");
	}
	before_used = region.used;
	if (jsval_promote_array_shallow_measure(&region, scores, 4, &bytes) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_array_shallow_measure(scores)");
	}
	if (jsval_promote_array_shallow_in_place(&region, &scores, 4) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_array_shallow_in_place(scores)");
	}
	if (!jsval_is_native(scores)) {
		return generated_failf(detail, cap,
				"expected shallow-promoted scores array to be native");
	}
	if (region.used != before_used + bytes) {
		return generated_failf(detail, cap,
				"unexpected byte count from shallow array promotion");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"scores", 6,
			scores) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(scores)");
	}
	if (jsval_array_push(&region, scores, jsval_number(3.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(scores)");
	}
	if (jsval_array_set_length(&region, scores, 4) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_set_length(scores)");
	}
	if (jsval_array_get(&region, scores, 3, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(scores[3])");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected grown dense slot to read back as undefined");
	}
	if (jsval_array_set(&region, scores, 3, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(scores[3])");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_native_containers(char *detail,
		size_t cap)
{
	static const uint8_t expected_json[] = "{\"keep\":true,\"items\":[1,2]}";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t got;
	int has;
	int deleted;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 3, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new");
	}
	if (jsval_array_new(&region, 4, &items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(keep)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"drop", 4,
			jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(drop)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"items", 5,
			items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(items)");
	}

	if (jsval_object_has_own_utf8(&region, root, (const uint8_t *)"keep", 4,
			&has) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_has_own_utf8(keep)");
	}
	if (!has) {
		return generated_failf(detail, cap, "expected keep to be an own property");
	}
	if (jsval_object_has_own_utf8(&region, root, (const uint8_t *)"missing", 7,
			&has) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_has_own_utf8(missing)");
	}
	if (has) {
		return generated_failf(detail, cap,
				"missing property unexpectedly reported as present");
	}
	if (jsval_object_delete_utf8(&region, root, (const uint8_t *)"drop", 4,
			&deleted) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_delete_utf8(drop)");
	}
	if (!deleted) {
		return generated_failf(detail, cap, "expected drop deletion to report success");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"drop", 4, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(drop)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap, "expected deleted property to read back as undefined");
	}

	if (jsval_array_push(&region, items, jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(1)");
	}
	if (jsval_array_push(&region, items, jsval_number(2.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(2)");
	}
	if (jsval_array_set_length(&region, items, 4) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set_length(grow)");
	}
	if (jsval_array_get(&region, items, 2, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(2)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap, "expected grown dense slot to be undefined");
	}
	if (jsval_array_set_length(&region, items, 2) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set_length(shrink)");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_lookup_capacity_contracts(
		char *detail, size_t cap)
{
	static const uint8_t input[] =
		"{\"keep\":7,\"items\":[1,2],\"nested\":{\"flag\":true}}";
	static const uint8_t expected_json[] =
		"{\"keep\":9,\"items\":[1,8,3],\"nested\":{\"flag\":true}}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t got;
	size_t bytes;
	size_t object_size;
	size_t array_len;
	int has;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 24, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"missing", 7,
			&got) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(missing)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected missing property read to return undefined");
	}
	if (jsval_object_has_own_utf8(&region, root, (const uint8_t *)"keep", 4,
			&has) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_has_own_utf8(keep)");
	}
	if (!has) {
		return generated_failf(detail, cap,
				"expected keep to be an own property before promotion");
	}
	if (jsval_object_has_own_utf8(&region, root, (const uint8_t *)"missing", 7,
			&has) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_has_own_utf8(missing)");
	}
	if (has) {
		return generated_failf(detail, cap,
				"missing property unexpectedly reported as present");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(items)");
	}
	if (jsval_array_get(&region, items, 4, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(items[4])");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected out-of-range JSON-backed array read to return undefined");
	}

	errno = 0;
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_number(9.0)) == 0) {
		return generated_failf(detail, cap,
				"JSON-backed object overwrite unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP before object promotion, got %d", errno);
	}
	errno = 0;
	if (jsval_array_push(&region, items, jsval_number(3.0)) == 0) {
		return generated_failf(detail, cap,
				"JSON-backed array push unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP before array promotion, got %d", errno);
	}
	errno = 0;
	if (jsval_promote_object_shallow_measure(&region, root, 2, &bytes) == 0) {
		return generated_failf(detail, cap,
				"undersized shallow object plan unexpectedly succeeded");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from undersized object plan, got %d", errno);
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(root)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root");
	}
	object_size = jsval_object_size(&region, root);
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"keep", 4,
			jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(keep)");
	}
	if (jsval_object_size(&region, root) != object_size) {
		return generated_failf(detail, cap,
				"object overwrite unexpectedly changed object size");
	}

	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5,
			&items) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(items after root promotion)");
	}
	errno = 0;
	if (jsval_promote_array_shallow_measure(&region, items, 1, &bytes) == 0) {
		return generated_failf(detail, cap,
				"undersized shallow array plan unexpectedly succeeded");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from undersized array plan, got %d", errno);
	}
	if (jsval_promote_array_shallow_in_place(&region, &items, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_array_shallow_in_place(items)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"items", 5,
			items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(items)");
	}
	array_len = jsval_array_length(&region, items);
	if (jsval_array_set(&region, items, 1, jsval_number(8.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(items[1])");
	}
	if (jsval_array_length(&region, items) != array_len) {
		return generated_failf(detail, cap,
				"array overwrite unexpectedly changed logical length");
	}
	if (jsval_array_push(&region, items, jsval_number(3.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(items)");
	}
	errno = 0;
	if (jsval_array_push(&region, items, jsval_number(4.0)) == 0) {
		return generated_failf(detail, cap,
				"push past planned capacity unexpectedly succeeded");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from push past capacity, got %d", errno);
	}
	errno = 0;
	if (jsval_array_set_length(&region, items, 4) == 0) {
		return generated_failf(detail, cap,
				"length past planned capacity unexpectedly succeeded");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from length past capacity, got %d", errno);
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_object_key_order(char *detail,
		size_t cap)
{
	static const uint8_t json_input[] = "{\"z\":1,\"a\":2,\"m\":3}";
	static const uint8_t expected_json[] =
		"{\"native\":[\"keep\",\"items\"],\"json\":[\"z\",\"a\",\"m\"]}";
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t output;
	jsval_t native_object;
	jsval_t native_items;
	jsval_t native_keys;
	jsval_t parsed_object;
	jsval_t parsed_keys;
	jsval_t key;
	int deleted;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 2, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}
	if (jsval_object_new(&region, 3, &native_object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(native)");
	}
	if (jsval_array_new(&region, 1, &native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native_items)");
	}
	if (jsval_object_set_utf8(&region, native_object, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(keep)");
	}
	if (jsval_object_set_utf8(&region, native_object, (const uint8_t *)"drop", 4,
			jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(drop)");
	}
	if (jsval_object_set_utf8(&region, native_object, (const uint8_t *)"items", 5,
			native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(items)");
	}
	if (jsval_object_set_utf8(&region, native_object, (const uint8_t *)"drop", 4,
			jsval_number(8.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(overwrite drop)");
	}
	if (jsval_object_key_at(&region, native_object, 0, &key) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_key_at(native 0)");
	}
	status = generated_expect_string(&region, key, (const uint8_t *)"keep", 4,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_key_at(&region, native_object, 1, &key) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_key_at(native 1)");
	}
	status = generated_expect_string(&region, key, (const uint8_t *)"drop", 4,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_key_at(&region, native_object, 2, &key) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_key_at(native 2)");
	}
	status = generated_expect_string(&region, key, (const uint8_t *)"items", 5,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_delete_utf8(&region, native_object,
			(const uint8_t *)"drop", 4, &deleted) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_delete_utf8(drop)");
	}
	if (!deleted) {
		return generated_failf(detail, cap,
				"expected drop deletion to report success");
	}
	if (generated_build_object_keys_array(&region, native_object, &native_keys) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_build_object_keys_array(native)");
	}
	if (jsval_json_parse(&region, json_input, sizeof(json_input) - 1, 16,
			&parsed_object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (generated_build_object_keys_array(&region, parsed_object, &parsed_keys) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_build_object_keys_array(json)");
	}
	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"native", 6,
			native_keys) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(native)");
	}
	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"json", 4,
			parsed_keys) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(json)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_object_value_order(char *detail,
		size_t cap)
{
	static const uint8_t json_input[] = "{\"z\":1,\"a\":2,\"m\":3}";
	static const uint8_t expected_json[] =
		"{\"nativeValues\":[true,[]],\"nativeEntries\":[[\"keep\",true],[\"items\",[]]],\"jsonValues\":[1,2,3],\"jsonEntries\":[[\"z\",1],[\"a\",2],[\"m\",3]]}";
	uint8_t storage[12288];
	jsval_region_t region;
	jsval_t output;
	jsval_t native_object;
	jsval_t native_items;
	jsval_t native_values;
	jsval_t native_entries;
	jsval_t parsed_object;
	jsval_t parsed_values;
	jsval_t parsed_entries;
	jsval_t value;
	int deleted;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 4, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}
	if (jsval_object_new(&region, 3, &native_object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(native)");
	}
	if (jsval_array_new(&region, 1, &native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native_items)");
	}
	if (jsval_object_set_utf8(&region, native_object, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0
			|| jsval_object_set_utf8(&region, native_object,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) < 0
			|| jsval_object_set_utf8(&region, native_object,
			(const uint8_t *)"items", 5, native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(native)");
	}
	if (jsval_object_set_utf8(&region, native_object, (const uint8_t *)"drop", 4,
			jsval_number(8.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(overwrite drop)");
	}
	if (jsval_object_value_at(&region, native_object, 1, &value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_value_at(native overwrite)");
	}
	status = generated_expect_number(&region, value, 8.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_delete_utf8(&region, native_object,
			(const uint8_t *)"drop", 4, &deleted) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_delete_utf8(drop)");
	}
	if (!deleted) {
		return generated_failf(detail, cap,
				"expected drop deletion to report success");
	}
	if (generated_build_object_values_array(&region, native_object, &native_values) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_build_object_values_array(native)");
	}
	if (generated_build_object_entries_array(&region, native_object, &native_entries) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_build_object_entries_array(native)");
	}
	if (jsval_json_parse(&region, json_input, sizeof(json_input) - 1, 16,
			&parsed_object) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (generated_build_object_values_array(&region, parsed_object, &parsed_values) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_build_object_values_array(json)");
	}
	if (generated_build_object_entries_array(&region, parsed_object, &parsed_entries) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_build_object_entries_array(json)");
	}
	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"nativeValues", 12,
			native_values) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"nativeEntries", 13, native_entries) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonValues", 10, parsed_values) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonEntries", 11, parsed_entries) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_object_copy_own(char *detail,
		size_t cap)
{
	static const uint8_t json_source[] = "{\"tail\":9,\"last\":10,\"drop\":11}";
	static const uint8_t fail_source[] = "{\"a\":1,\"b\":2}";
	static const uint8_t expected_json[] =
		"{\"merged\":{\"keep\":true,\"drop\":11,\"items\":[],\"tail\":9,\"last\":10},\"unchanged\":{\"keep\":true}}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t output;
	jsval_t merged;
	jsval_t native_src;
	jsval_t native_items;
	jsval_t parsed_src;
	jsval_t tight_dst;
	jsval_t fail_src;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 2, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}
	if (jsval_object_new(&region, 5, &merged) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(merged)");
	}
	if (jsval_object_new(&region, 2, &native_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(native_src)");
	}
	if (jsval_array_new(&region, 0, &native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native_items)");
	}
	if (jsval_object_set_utf8(&region, merged, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0
			|| jsval_object_set_utf8(&region, merged,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(merged)");
	}
	if (jsval_object_set_utf8(&region, native_src, (const uint8_t *)"drop", 4,
			jsval_number(8.0)) < 0
			|| jsval_object_set_utf8(&region, native_src,
			(const uint8_t *)"items", 5, native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(native_src)");
	}
	if (jsval_object_copy_own(&region, merged, native_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_copy_own(native)");
	}
	if (jsval_json_parse(&region, json_source, sizeof(json_source) - 1, 8,
			&parsed_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(json_source)");
	}
	if (jsval_object_copy_own(&region, merged, parsed_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_copy_own(json)");
	}

	if (jsval_object_new(&region, 1, &tight_dst) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(tight_dst)");
	}
	if (jsval_object_set_utf8(&region, tight_dst, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(tight_dst)");
	}
	if (jsval_json_parse(&region, fail_source, sizeof(fail_source) - 1, 8,
			&fail_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(fail_source)");
	}
	errno = 0;
	if (jsval_object_copy_own(&region, tight_dst, fail_src) != -1) {
		return generated_failf(detail, cap,
				"expected copy into undersized destination to fail");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from undersized destination, got %d", errno);
	}

	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"merged", 6,
			merged) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"unchanged", 9, tight_dst) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_object_clone_own(char *detail,
		size_t cap)
{
	static const uint8_t json_source[] = "{\"z\":1,\"a\":2}";
	static const uint8_t merge_source[] = "{\"drop\":9,\"tail\":10}";
	static const uint8_t fail_source[] = "{\"a\":1,\"b\":2}";
	static const uint8_t expected_json[] =
		"{\"jsonSource\":{\"z\":1,\"a\":2},\"jsonClone\":{\"z\":7,\"a\":2},\"merged\":{\"keep\":true,\"drop\":9,\"items\":[],\"tail\":10}}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t output;
	jsval_t parsed_src;
	jsval_t json_clone;
	jsval_t native_src;
	jsval_t native_items;
	jsval_t merged;
	jsval_t merge_src;
	jsval_t fail_out;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 3, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}
	if (jsval_json_parse(&region, json_source, sizeof(json_source) - 1, 8,
			&parsed_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(json_source)");
	}
	if (jsval_object_clone_own(&region, parsed_src, 2, &json_clone) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_clone_own(json)");
	}
	if (jsval_object_set_utf8(&region, json_clone, (const uint8_t *)"z", 1,
			jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(json_clone)");
	}

	if (jsval_object_new(&region, 3, &native_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(native_src)");
	}
	if (jsval_array_new(&region, 0, &native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native_items)");
	}
	if (jsval_object_set_utf8(&region, native_src, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0
			|| jsval_object_set_utf8(&region, native_src,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) < 0
			|| jsval_object_set_utf8(&region, native_src,
			(const uint8_t *)"items", 5, native_items) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(native_src)");
	}
	if (jsval_object_clone_own(&region, native_src, 4, &merged) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_clone_own(native)");
	}
	if (jsval_json_parse(&region, merge_source, sizeof(merge_source) - 1, 8,
			&merge_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(merge_source)");
	}
	if (jsval_object_copy_own(&region, merged, merge_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_copy_own(merge)");
	}

	fail_out = jsval_null();
	errno = 0;
	if (jsval_json_parse(&region, fail_source, sizeof(fail_source) - 1, 8,
			&merge_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(fail_source)");
	}
	if (jsval_object_clone_own(&region, merge_src, 1, &fail_out) != -1) {
		return generated_failf(detail, cap,
				"expected clone with undersized capacity to fail");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from undersized clone capacity, got %d",
				errno);
	}
	if (fail_out.kind != JSVAL_KIND_NULL) {
		return generated_failf(detail, cap,
				"expected failed clone output to remain unchanged");
	}

	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"jsonSource", 10,
			parsed_src) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonClone", 9, json_clone) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"merged", 6, merged) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_object_spread_parity(
		char *detail, size_t cap)
{
	static const uint8_t json_source[] = "{\"z\":1,\"a\":2}";
	static const uint8_t merge_source[] = "{\"drop\":9,\"tail\":10}";
	static const uint8_t around_source[] = "{\"mid\":7,\"tail\":8}";
	static const uint8_t expected_json[] =
		"{\"nativeSrc\":{\"keep\":1,\"nested\":[]},\"nativeClone\":{\"keep\":7,\"nested\":[]},\"jsonClone\":{\"z\":1,\"a\":2},\"merged\":{\"keep\":true,\"drop\":9,\"items\":[],\"tail\":10},\"around\":{\"head\":1,\"mid\":7,\"tail\":2}}";
	uint8_t storage[24576];
	jsval_region_t region;
	jsval_t output;
	jsval_t native_src;
	jsval_t nested;
	jsval_t native_clone;
	jsval_t parsed_src;
	jsval_t json_clone;
	jsval_t first;
	jsval_t items;
	jsval_t second;
	jsval_t merged;
	jsval_t around_src;
	jsval_t around;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 5, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}

	if (jsval_object_new(&region, 2, &native_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(native_src)");
	}
	if (jsval_array_new(&region, 0, &nested) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(nested)");
	}
	if (jsval_object_set_utf8(&region, native_src, (const uint8_t *)"keep", 4,
			jsval_number(1.0)) < 0
			|| jsval_object_set_utf8(&region, native_src,
			(const uint8_t *)"nested", 6, nested) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(native_src)");
	}
	if (jsval_object_clone_own(&region, native_src, 2, &native_clone) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_clone_own(native_clone)");
	}
	if (jsval_object_set_utf8(&region, native_clone, (const uint8_t *)"keep", 4,
			jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(native_clone)");
	}

	if (jsval_json_parse(&region, json_source, sizeof(json_source) - 1, 8,
			&parsed_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(json_source)");
	}
	if (jsval_object_clone_own(&region, parsed_src, 2, &json_clone) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_clone_own(json_clone)");
	}

	if (jsval_object_new(&region, 3, &first) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(first)");
	}
	if (jsval_array_new(&region, 0, &items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(items)");
	}
	if (jsval_object_set_utf8(&region, first, (const uint8_t *)"keep", 4,
			jsval_bool(1)) < 0
			|| jsval_object_set_utf8(&region, first,
			(const uint8_t *)"drop", 4, jsval_number(7.0)) < 0
			|| jsval_object_set_utf8(&region, first,
			(const uint8_t *)"items", 5, items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(first)");
	}
	if (jsval_json_parse(&region, merge_source, sizeof(merge_source) - 1, 8,
			&second) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(merge_source)");
	}
	if (jsval_object_clone_own(&region, first, 4, &merged) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_clone_own(merged)");
	}
	if (jsval_object_copy_own(&region, merged, second) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_copy_own(merged)");
	}

	if (jsval_json_parse(&region, around_source, sizeof(around_source) - 1, 8,
			&around_src) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(around_source)");
	}
	if (jsval_object_new(&region, 3, &around) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(around)");
	}
	if (jsval_object_set_utf8(&region, around, (const uint8_t *)"head", 4,
			jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(around head)");
	}
	if (jsval_object_copy_own(&region, around, around_src) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_copy_own(around)");
	}
	if (jsval_object_set_utf8(&region, around, (const uint8_t *)"tail", 4,
			jsval_number(2.0)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(around tail)");
	}

	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"nativeSrc", 9,
			native_src) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"nativeClone", 11, native_clone) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonClone", 9, json_clone) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"merged", 6, merged) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"around", 6, around) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_array_clone_dense(char *detail,
		size_t cap)
{
	static const uint8_t json_source[] = "[1,2]";
	static const uint8_t expected_json[] =
		"{\"jsonSource\":[1,2],\"jsonClone\":[7,2],\"nativeSource\":[4,5],\"nativeClone\":[4,5,6]}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t output;
	jsval_t parsed_src;
	jsval_t json_clone;
	jsval_t native_src;
	jsval_t native_clone;
	jsval_t fail_out;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 4, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}
	if (jsval_json_parse(&region, json_source, sizeof(json_source) - 1, 8,
			&parsed_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(json_source)");
	}
	if (jsval_array_clone_dense(&region, parsed_src, 2, &json_clone) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_clone_dense(json)");
	}
	if (jsval_array_set(&region, json_clone, 0, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(json_clone[0])");
	}

	if (jsval_array_new(&region, 2, &native_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native_src)");
	}
	if (jsval_array_push(&region, native_src, jsval_number(4.0)) < 0
			|| jsval_array_push(&region, native_src, jsval_number(5.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(native_src)");
	}
	if (jsval_array_clone_dense(&region, native_src, 3, &native_clone) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_clone_dense(native)");
	}
	if (jsval_array_push(&region, native_clone, jsval_number(6.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(native_clone)");
	}

	fail_out = jsval_null();
	errno = 0;
	if (jsval_array_clone_dense(&region, parsed_src, 1, &fail_out) != -1) {
		return generated_failf(detail, cap,
				"expected clone with undersized capacity to fail");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from undersized clone capacity, got %d",
				errno);
	}
	if (fail_out.kind != JSVAL_KIND_NULL) {
		return generated_failf(detail, cap,
				"expected failed clone output to remain unchanged");
	}

	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"jsonSource", 10,
			parsed_src) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonClone", 9, json_clone) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"nativeSource", 12, native_src) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"nativeClone", 11, native_clone) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_array_spread_parity(
		char *detail, size_t cap)
{
	static const uint8_t json_source[] = "[1,2]";
	static const uint8_t merge_source[] = "[3,4]";
	static const uint8_t expected_json[] =
		"{\"nativeSrc\":[4,5],\"nativeClone\":[9,5],\"jsonSrc\":[1,2],\"jsonClone\":[1,2,3],\"merged\":[1,2,3,4],\"around\":[0,1,2,9]}";
	uint8_t storage[24576];
	jsval_region_t region;
	jsval_t output;
	jsval_t native_src;
	jsval_t native_clone;
	jsval_t parsed_src;
	jsval_t json_clone;
	jsval_t merge_src;
	jsval_t merged;
	jsval_t around;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 6, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}

	if (jsval_array_new(&region, 2, &native_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native_src)");
	}
	if (jsval_array_push(&region, native_src, jsval_number(4.0)) < 0
			|| jsval_array_push(&region, native_src, jsval_number(5.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(native_src)");
	}
	if (jsval_array_clone_dense(&region, native_src, 2, &native_clone) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_clone_dense(native_clone)");
	}
	if (jsval_array_set(&region, native_clone, 0, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native_clone)");
	}

	if (jsval_json_parse(&region, json_source, sizeof(json_source) - 1, 8,
			&parsed_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(json_source)");
	}
	if (jsval_array_clone_dense(&region, parsed_src, 3, &json_clone) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_clone_dense(json_clone)");
	}
	if (jsval_array_push(&region, json_clone, jsval_number(3.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(json_clone)");
	}

	if (jsval_json_parse(&region, merge_source, sizeof(merge_source) - 1, 8,
			&merge_src) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(merge_source)");
	}
	if (jsval_array_clone_dense(&region, parsed_src, 4, &merged) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_clone_dense(merged)");
	}
	if (generated_append_dense_array(&region, merged, merge_src) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_append_dense_array(merged)");
	}

	if (jsval_array_new(&region, 4, &around) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(around)");
	}
	if (jsval_array_push(&region, around, jsval_number(0.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(around head)");
	}
	if (generated_append_dense_array(&region, around, parsed_src) < 0) {
		return generated_fail_errno(detail, cap,
				"generated_append_dense_array(around)");
	}
	if (jsval_array_push(&region, around, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(around tail)");
	}

	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"nativeSrc", 9,
			native_src) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"nativeClone", 11, native_clone) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonSrc", 7, parsed_src) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"jsonClone", 9, json_clone) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"merged", 6, merged) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"around", 6, around) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_array_splice_dense(char *detail,
		size_t cap)
{
	static const uint8_t expected_json[] =
		"{\"deleteTarget\":[1,4],\"deleteRemoved\":[2,3],\"replaceTarget\":[1,9,4],\"replaceRemoved\":[2,3],\"unchanged\":[1,2,3,4]}";
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t output;
	jsval_t delete_target;
	jsval_t delete_removed;
	jsval_t replace_target;
	jsval_t replace_removed;
	jsval_t unchanged;
	jsval_t fail_removed;
	jsval_t got;
	jsval_t insert_one[] = {jsval_number(9.0)};
	jsval_t insert_three[] = {
		jsval_number(7.0),
		jsval_number(8.0),
		jsval_number(9.0)
	};

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_object_new(&region, 5, &output) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(output)");
	}
	if (jsval_array_new(&region, 4, &delete_target) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(delete_target)");
	}
	if (jsval_array_push(&region, delete_target, jsval_number(1.0)) < 0
			|| jsval_array_push(&region, delete_target, jsval_number(2.0)) < 0
			|| jsval_array_push(&region, delete_target, jsval_number(3.0)) < 0
			|| jsval_array_push(&region, delete_target, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(delete_target)");
	}
	if (jsval_array_splice_dense(&region, delete_target, 1, 2, NULL, 0,
			&delete_removed) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_splice_dense(delete)");
	}

	if (jsval_array_new(&region, 4, &replace_target) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(replace_target)");
	}
	if (jsval_array_push(&region, replace_target, jsval_number(1.0)) < 0
			|| jsval_array_push(&region, replace_target, jsval_number(2.0)) < 0
			|| jsval_array_push(&region, replace_target, jsval_number(3.0)) < 0
			|| jsval_array_push(&region, replace_target, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(replace_target)");
	}
	if (jsval_array_splice_dense(&region, replace_target, 1, 2, insert_one, 1,
			&replace_removed) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_splice_dense(replace)");
	}
	if (jsval_array_get(&region, replace_target, 3, &got) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(replace_target[3])");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected vacated dense tail to read as undefined after shrink");
	}

	if (jsval_array_new(&region, 4, &unchanged) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(unchanged)");
	}
	if (jsval_array_push(&region, unchanged, jsval_number(1.0)) < 0
			|| jsval_array_push(&region, unchanged, jsval_number(2.0)) < 0
			|| jsval_array_push(&region, unchanged, jsval_number(3.0)) < 0
			|| jsval_array_push(&region, unchanged, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(unchanged)");
	}
	fail_removed = jsval_null();
	errno = 0;
	if (jsval_array_splice_dense(&region, unchanged, 1, 1, insert_three, 3,
			&fail_removed) != -1) {
		return generated_failf(detail, cap,
				"expected splice into undersized dense array to fail");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from undersized dense splice, got %d", errno);
	}
	if (fail_removed.kind != JSVAL_KIND_NULL) {
		return generated_failf(detail, cap,
				"expected failed dense splice removed output to remain unchanged");
	}

	if (jsval_object_set_utf8(&region, output, (const uint8_t *)"deleteTarget", 12,
			delete_target) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"deleteRemoved", 13, delete_removed) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"replaceTarget", 13, replace_target) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"replaceRemoved", 14, replace_removed) < 0
			|| jsval_object_set_utf8(&region, output,
			(const uint8_t *)"unchanged", 9, unchanged) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(output)");
	}

	return generated_expect_json(&region, output, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_dense_array_semantics(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "[4,5]";
	static const uint8_t expected_json[] =
		"{\"native\":[1,9],\"parsed\":[4,5]}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t native_items;
	jsval_t parsed_items;
	jsval_t tight_items;
	jsval_t got;
	size_t bytes;
	size_t len_before;

	jsval_region_init(&region, storage, sizeof(storage));

	if (jsval_json_parse(&region, input, sizeof(input) - 1, 8, &parsed_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (!jsval_is_json_backed(parsed_items)) {
		return generated_failf(detail, cap,
				"expected parsed array to start JSON-backed");
	}
	if (jsval_array_get(&region, parsed_items, 1, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(parsed[1])");
	}
	if (generated_expect_number(&region, got, 5.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	errno = 0;
	if (jsval_array_pop(&region, parsed_items, &got) == 0) {
		return generated_failf(detail, cap,
				"expected JSON-backed parsed array pop to fail before promotion");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP from parsed array pop, got %d", errno);
	}
	errno = 0;
	if (jsval_array_shift(&region, parsed_items, &got) == 0) {
		return generated_failf(detail, cap,
				"expected JSON-backed parsed array shift to fail before promotion");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP from parsed array shift, got %d", errno);
	}
	errno = 0;
	if (jsval_array_unshift(&region, parsed_items, jsval_number(4.0)) == 0) {
		return generated_failf(detail, cap,
				"expected JSON-backed parsed array unshift to fail before promotion");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap,
				"expected ENOTSUP from parsed array unshift, got %d", errno);
	}
	if (jsval_promote_array_shallow_measure(&region, parsed_items, 3, &bytes) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_array_shallow_measure(parsed)");
	}
	if (bytes == 0) {
		return generated_failf(detail, cap,
				"expected shallow promotion with extra capacity to consume bytes");
	}
	if (jsval_promote_array_shallow_in_place(&region, &parsed_items, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_array_shallow_in_place(parsed)");
	}
	if (!jsval_is_native(parsed_items)) {
		return generated_failf(detail, cap,
				"expected shallow-promoted parsed array to become native");
	}
	if (jsval_array_push(&region, parsed_items, jsval_number(6.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(parsed)");
	}
	if (jsval_array_pop(&region, parsed_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_pop(parsed)");
	}
	if (generated_expect_number(&region, got, 6.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_array_shift(&region, parsed_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_shift(parsed)");
	}
	if (generated_expect_number(&region, got, 4.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_array_unshift(&region, parsed_items, jsval_number(4.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_unshift(parsed)");
	}

	if (jsval_array_new(&region, 4, &native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(native)");
	}
	if (jsval_array_set(&region, native_items, 0, jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[0])");
	}
	if (jsval_array_set(&region, native_items, 1, jsval_number(2.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[1])");
	}
	len_before = jsval_array_length(&region, native_items);
	if (jsval_array_set(&region, native_items, 1, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[1]=9)");
	}
	if (jsval_array_length(&region, native_items) != len_before) {
		return generated_failf(detail, cap,
				"native overwrite unexpectedly changed logical length");
	}
	if (jsval_array_set_length(&region, native_items, 4) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set_length(native grow)");
	}
	if (jsval_array_get(&region, native_items, 2, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(native[2])");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected grown native dense slot to read as undefined");
	}
	if (jsval_array_set(&region, native_items, 2, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[2]=7)");
	}
	if (jsval_array_set(&region, native_items, 3, jsval_number(8.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[3]=8)");
	}
	if (jsval_array_set_length(&region, native_items, 2) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set_length(native shrink)");
	}
	if (jsval_array_get(&region, native_items, 2, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_get(native[2] after shrink)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected truncated native slot to read as undefined");
	}
	if (jsval_array_pop(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_pop(native first)");
	}
	if (generated_expect_number(&region, got, 9.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_array_pop(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_pop(native second)");
	}
	if (generated_expect_number(&region, got, 1.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_array_pop(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_pop(native empty)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected empty native pop to return undefined");
	}
	if (jsval_array_set_length(&region, native_items, 2) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_set_length(native regrow)");
	}
	if (jsval_array_pop(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_pop(native grown undefined tail)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected grown undefined tail pop to return undefined");
	}
	if (jsval_array_pop(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_pop(native final undefined tail)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected final native pop to return undefined");
	}
	if (jsval_array_set(&region, native_items, 0, jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[0] reset)");
	}
	if (jsval_array_set(&region, native_items, 1, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set(native[1] reset)");
	}
	if (jsval_array_shift(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_shift(native first)");
	}
	if (generated_expect_number(&region, got, 1.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_array_shift(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_shift(native second)");
	}
	if (generated_expect_number(&region, got, 9.0, detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_array_shift(&region, native_items, &got) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_shift(native empty)");
	}
	if (got.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected empty native shift to return undefined");
	}
	if (jsval_array_unshift(&region, native_items, jsval_number(9.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_unshift(native tail)");
	}
	if (jsval_array_unshift(&region, native_items, jsval_number(1.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_unshift(native head)");
	}

	if (jsval_array_new(&region, 1, &tight_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_new(tight)");
	}
	if (jsval_array_push(&region, tight_items, jsval_number(5.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_push(tight)");
	}
	errno = 0;
	if (jsval_array_unshift(&region, tight_items, jsval_number(4.0)) == 0) {
		return generated_failf(detail, cap,
				"expected native unshift past capacity to fail");
	}
	if (errno != ENOBUFS) {
		return generated_failf(detail, cap,
				"expected ENOBUFS from native unshift past capacity, got %d", errno);
	}

	if (jsval_object_new(&region, 2, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_new(root)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"native", 6,
			native_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(native)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"parsed", 6,
			parsed_items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(parsed)");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_locale_upper(char *detail,
		size_t cap)
{
	static const uint8_t input[] = "{\"city\":\"istanbul\"}";
	static const uint8_t expected_string[] = {
		0xC4, 0xB0, 'S', 'T', 'A', 'N', 'B', 'U', 'L'
	};
	static const uint8_t expected_json[] =
		"{\"city\":\"\xC4\xB0STANBUL\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t city;
	jsval_t locale;
	jsval_t upper;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"city", 4, &city) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(city)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"tr", 2, &locale) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(locale)");
	}
	if (jsval_method_string_to_locale_upper_case(&region, city, 1, locale,
			&upper, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_to_locale_upper_case failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	status = generated_expect_string(&region, upper, expected_string,
			sizeof(expected_string), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_region_promote_root(&region, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_promote_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"city", 4, upper) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(city)");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_normalize(char *detail,
		size_t cap)
{
	static const uint8_t input[] = "{\"name\":\"A\xCC\x8A\"}";
	static const uint8_t expected_string[] = {0xC3, 0x85};
	static const uint8_t expected_json[] = "{\"name\":\"\xC3\x85\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t name;
	jsval_t normalized;
	jsmethod_error_t error;
	jsmethod_string_normalize_sizes_t sizes;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"name", 4, &name) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(name)");
	}
	if (jsval_method_string_normalize_measure(&region, name, 0,
			jsval_undefined(), &sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_normalize_measure failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	{
		uint16_t this_storage[sizes.this_storage_len ? sizes.this_storage_len : 1];
		uint32_t workspace[sizes.workspace_len ? sizes.workspace_len : 1];

		if (jsval_method_string_normalize(&region, name, 0, jsval_undefined(),
				this_storage, sizes.this_storage_len,
				NULL, 0,
				workspace, sizes.workspace_len,
				&normalized, &error) < 0) {
			return generated_failf(detail, cap,
					"jsval_method_string_normalize failed: errno=%d kind=%d",
					errno, (int)error.kind);
		}
	}

	status = generated_expect_string(&region, normalized, expected_string,
			sizeof(expected_string), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_region_promote_root(&region, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_promote_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"name", 4,
			normalized) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(name)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_lower(char *detail,
		size_t cap)
{
	static const uint8_t input[] = "{\"message\":\"Hello, WoRlD!\"}";
	static const uint8_t expected_string[] = "hello, world!";
	static const uint8_t expected_json[] = "{\"message\":\"hello, world!\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t message;
	jsval_t lower;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"message", 7,
			&message) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(message)");
	}
	if (jsval_method_string_to_lower_case(&region, message, &lower,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_to_lower_case failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	status = generated_expect_string(&region, lower, expected_string,
			sizeof(expected_string) - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 7) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"message", 7,
			lower) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(message)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_is_well_formed(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"kind\":\"probe\",\"isWellFormed\":true}";
	static const uint16_t broken_units[] = {0xD83D};
	static const uint8_t expected_json[] =
		"{\"kind\":\"probe\",\"isWellFormed\":false}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t broken;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_string_new_utf16(&region, broken_units,
			sizeof(broken_units) / sizeof(broken_units[0]), &broken) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf16");
	}
	if (jsval_method_string_is_well_formed(&region, broken, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_is_well_formed failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (result.kind != JSVAL_KIND_BOOL || result.as.boolean != 0) {
		return generated_failf(detail, cap,
				"expected isWellFormed result to be false");
	}
	if (jsval_region_promote_root(&region, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_promote_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"isWellFormed",
			12, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(isWellFormed)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_search(char *detail,
		size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"bananas\",\"needle\":\"na\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"bananas\",\"needle\":\"na\",\"index\":2,\"last\":2,\"includes\":true,\"starts\":true,\"ends\":true}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t needle;
	jsval_t pos_three;
	jsval_t index_result;
	jsval_t last_result;
	jsval_t includes_result;
	jsval_t starts_result;
	jsval_t ends_result;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"needle", 6,
			&needle) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(needle)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"3", 1, &pos_three) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(pos)");
	}
	if (jsval_method_string_index_of(&region, text, needle, 0,
			jsval_undefined(), &index_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_index_of failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_last_index_of(&region, text, needle, 1, pos_three,
			&last_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_last_index_of failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_includes(&region, text, needle, 1, pos_three,
			&includes_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_includes failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_starts_with(&region, text, needle, 1,
			jsval_number(2.0), &starts_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_starts_with failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_ends_with(&region, text, needle, 1,
			jsval_number(4.0), &ends_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_ends_with failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	status = generated_expect_number(&region, index_result, 2.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_number(&region, last_result, 2.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (includes_result.kind != JSVAL_KIND_BOOL || !includes_result.as.boolean) {
		return generated_failf(detail, cap, "expected includes result to be true");
	}
	if (starts_result.kind != JSVAL_KIND_BOOL || !starts_result.as.boolean) {
		return generated_failf(detail, cap, "expected startsWith result to be true");
	}
	if (ends_result.kind != JSVAL_KIND_BOOL || !ends_result.as.boolean) {
		return generated_failf(detail, cap, "expected endsWith result to be true");
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 7) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"index", 5,
			index_result) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"last", 4,
			last_result) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"includes", 8,
			includes_result) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"starts", 6,
			starts_result) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"ends", 4,
			ends_result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(search)");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_search_abrupt(
		char *detail, size_t cap)
{
	static const uint8_t bananas_utf8[] = "bananas";
	static const uint8_t needle_utf8[] = "na";
	generated_callback_ctx_t throw_ctx = {1};
	jsmethod_error_t error;
	int result;

	if (jsmethod_string_includes(&result,
			jsmethod_value_string_utf8(bananas_utf8,
				sizeof(bananas_utf8) - 1),
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt searchString coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT searchString coercion, got %d",
				(int)error.kind);
	}
	if (jsmethod_string_starts_with(&result,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			jsmethod_value_string_utf8(needle_utf8,
				sizeof(needle_utf8) - 1),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT receiver coercion, got %d",
				(int)error.kind);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_method_split(char *detail,
		size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"a,b,,c\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a,b,,c\",\"parts\":[\"a\",\"b\",\"\",\"c\"],\"limit\":[\"a\",\"b\"]}";
	static const char *expected_parts[] = {"a", "b", "", "c"};
	static const char *expected_limit[] = {"a", "b"};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t comma;
	jsval_t limit_two;
	jsval_t parts;
	jsval_t limited;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)",", 1, &comma) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(split)");
	}
	if (jsval_method_string_split(&region, text, 1, comma, 0,
			jsval_undefined(), &parts, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split(parts) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string_array(&region, parts, expected_parts,
			sizeof(expected_parts) / sizeof(expected_parts[0]), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_method_string_split(&region, text, 1, comma, 1, limit_two,
			&limited, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split(limit) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string_array(&region, limited, expected_limit,
			sizeof(expected_limit) / sizeof(expected_limit[0]), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(split)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root(split)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"parts", 5,
			parts) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"limit", 5, limited) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(split)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_split_abrupt(
		char *detail, size_t cap)
{
	static const uint16_t comma_units[] = {','};
	generated_callback_ctx_t throw_ctx = {1};
	int separator_calls = 0;
	generated_callback_ctx_t later_separator_ctx = {0, comma_units,
			sizeof(comma_units) / sizeof(comma_units[0]), &separator_calls};
	jsmethod_error_t error;
	size_t count = 0;

	if (jsmethod_string_split(
			jsmethod_value_string_utf8((const uint8_t *)"a,b", 3),
			1, jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			0, jsmethod_value_undefined(),
			NULL, NULL, &count, &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt separator coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT separator coercion, got %d",
				(int)error.kind);
	}
	if (jsmethod_string_split(
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			1, jsmethod_value_coercible(&later_separator_ctx,
					generated_callback_to_string),
			0, jsmethod_value_undefined(),
			NULL, NULL, &count, &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT receiver coercion, got %d",
				(int)error.kind);
	}
	if (separator_calls != 0) {
		return generated_failf(detail, cap,
				"expected later separator coercion to remain unevaluated, got %d calls",
				separator_calls);
	}
	return GENERATED_PASS;
}

#if JSMX_WITH_REGEX
static generated_status_t generated_smoke_jsval_method_regex_split(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"a,b,c\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a,b,c\",\"capture\":[\"a\",\",\",\"b\",\",\",\"c\"],\"zero\":[\"a\",\"b\"]}";
	static const char *expected_capture[] = {"a", ",", "b", ",", "c"};
	static const char *expected_zero[] = {"a", "b"};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t capture_pattern;
	jsval_t zero_pattern;
	jsval_t capture_regex;
	jsval_t zero_regex;
	jsval_t native_ab;
	jsval_t capture_result;
	jsval_t zero_result;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(regex split)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"(,)", 3,
			&capture_pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"(?:)", 4,
			&zero_pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"ab", 2,
			&native_ab) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex split inputs)");
	}
	if (jsval_regexp_new(&region, capture_pattern, 0, jsval_undefined(),
			&capture_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(capture split) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_new(&region, zero_pattern, 0, jsval_undefined(),
			&zero_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(zero split) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_split(&region, text, 1, capture_regex, 0,
			jsval_undefined(), &capture_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split(capture regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string_array(&region, capture_result,
			expected_capture,
			sizeof(expected_capture) / sizeof(expected_capture[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_method_string_split(&region, native_ab, 1, zero_regex, 0,
			jsval_undefined(), &zero_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split(zero regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string_array(&region, zero_result,
			expected_zero, sizeof(expected_zero) / sizeof(expected_zero[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(regex split)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_region_set_root(regex split)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"capture", 7,
			capture_result) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"zero", 4,
				zero_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex split)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}
#endif

static generated_status_t generated_smoke_jsval_method_concat(char *detail,
		size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"lego\",\"suffix\":\"A\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"lego\",\"suffix\":\"A\",\"concat\":\"legoAnulltrue42\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t suffix;
	jsval_t args[4];
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_concat_sizes_t sizes;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"suffix", 6,
			&suffix) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(suffix)");
	}

	args[0] = suffix;
	args[1] = jsval_null();
	args[2] = jsval_bool(1);
	args[3] = jsval_number(42.0);
	if (jsval_method_string_concat_measure(&region, text, 4, args, &sizes,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_concat_measure failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 15) {
		return generated_failf(detail, cap,
				"expected concat result_len 15, got %zu", sizes.result_len);
	}
	if (jsval_method_string_concat(&region, text, 4, args, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_concat failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"legoAnulltrue42", 15, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"concat", 6,
			result) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8(concat)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_replace(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"text\":\"a1b2\",\"search\":\"1\",\"replacement\":\"X\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b2\",\"search\":\"1\",\"replacement\":\"X\",\"plain\":\"aXb2\",\"special\":\"a[$][1][a][b2]b2\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t search;
	jsval_t replacement;
	jsval_t special_replacement;
	jsval_t plain_result;
	jsval_t special_result;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"search", 6,
			&search) < 0
			|| jsval_object_get_utf8(&region, root,
				(const uint8_t *)"replacement", 11, &replacement) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(replace)");
	}
	if (jsval_method_string_replace_measure(&region, text, search, replacement,
			&sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_measure failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 4) {
		return generated_failf(detail, cap,
				"expected replace result_len 4, got %zu", sizes.result_len);
	}
	if (jsval_method_string_replace(&region, text, search, replacement,
			&plain_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, plain_result,
			(const uint8_t *)"aXb2", 4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"[$$][$&][$`][$']",
			16, &special_replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(special replacement)");
	}
	if (jsval_method_string_replace_measure(&region, text, search,
			special_replacement, &sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_measure(special) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 16) {
		return generated_failf(detail, cap,
				"expected special replace result_len 16, got %zu",
				sizes.result_len);
	}
	if (jsval_method_string_replace(&region, text, search, special_replacement,
			&special_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace(special) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, special_result,
			(const uint8_t *)"a[$][1][a][b2]b2", 16, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 5) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(replace)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"plain", 5,
			plain_result) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"special", 7, special_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(replace)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_replace_all(
		char *detail, size_t cap)
{
	static const uint8_t input[] =
		"{\"text\":\"a1b1\",\"search\":\"1\",\"replacement\":\"X\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b1\",\"search\":\"1\",\"replacement\":\"X\",\"plain\":\"aXbX\",\"special\":\"a[$][1]b[$][1]\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t search;
	jsval_t replacement;
	jsval_t special_replacement;
	jsval_t plain_result;
	jsval_t special_result;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"search", 6,
			&search) < 0
			|| jsval_object_get_utf8(&region, root,
				(const uint8_t *)"replacement", 11, &replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(replaceAll)");
	}
	if (jsval_method_string_replace_all_measure(&region, text, search,
			replacement, &sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_measure failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 4) {
		return generated_failf(detail, cap,
				"expected replaceAll result_len 4, got %zu",
				sizes.result_len);
	}
	if (jsval_method_string_replace_all(&region, text, search, replacement,
			&plain_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, plain_result,
			(const uint8_t *)"aXbX", 4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"[$$][$&]", 8,
			&special_replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(replaceAll special replacement)");
	}
	if (jsval_method_string_replace_all_measure(&region, text, search,
			special_replacement, &sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_measure(special) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 14) {
		return generated_failf(detail, cap,
				"expected special replaceAll result_len 14, got %zu",
				sizes.result_len);
	}
	if (jsval_method_string_replace_all(&region, text, search,
			special_replacement, &special_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all(special) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, special_result,
			(const uint8_t *)"a[$][1]b[$][1]", 14, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 5) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(replaceAll)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"plain", 5,
			plain_result) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"special", 7, special_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(replaceAll)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_concat_abrupt(
		char *detail, size_t cap)
{
	static const uint16_t arg_text[] = {'A'};
	generated_callback_ctx_t throw_ctx = {1, NULL, 0, NULL};
	int arg_calls = 0;
	generated_callback_ctx_t later_arg_ctx = {0, arg_text,
			sizeof(arg_text) / sizeof(arg_text[0]), &arg_calls};
	jsmethod_value_t args[2];
	uint16_t storage[16];
	jsmethod_error_t error;
	jsstr16_t out;

	args[0] = jsmethod_value_coercible(&later_arg_ctx,
			generated_callback_to_string);
	args[1] = jsmethod_value_undefined();
	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_concat(&out,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			2, args, &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT receiver coercion, got %d",
				(int)error.kind);
	}
	if (arg_calls != 0) {
		return generated_failf(detail, cap,
				"expected later concat args to remain unevaluated, got %d calls",
				arg_calls);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_method_replace_callback(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"a1b1\",\"search\":\"1\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b1\",\"search\":\"1\",\"callback\":\"a[1]b[3]\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t search;
	jsval_t result;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0
			|| jsval_object_get_utf8(&region, root, (const uint8_t *)"search", 6,
			&search) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(replace callback)");
	}
	if (jsval_method_string_replace_all_fn(&region, text, search,
			generated_replace_offset_callback, &ctx, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 callback calls, got %d", ctx.call_count);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"a[1]b[3]", 8, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(replace callback)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"callback", 8,
			result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(replace callback)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

#if JSMX_WITH_REGEX
static generated_status_t generated_smoke_jsval_method_regex_replace_callback(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"1b2\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"1b2\",\"callback\":\"<1b|1|b|0><2|2|U|2>\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t flags;
	jsval_t regex;
	jsval_t result;
	generated_regex_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 8, &root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(regex replace callback)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(regex replace callback)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])(b)?", 11,
			&pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&flags) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex replace callback args)");
	}
	if (jsval_regexp_new(&region, pattern, 1, flags, &regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(regex replace callback) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_replace_fn(&region, text, regex,
			generated_regex_replace_callback, &ctx, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_fn(regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 regex callback calls, got %d", ctx.call_count);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"<1b|1|b|0><2|2|U|2>", 19, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 2) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(regex replace callback)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"callback", 8,
			result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex replace callback)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_regex_replace_named_groups(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"a1b2\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t single_regex;
	jsval_t global_regex;
	jsval_t replacement;
	jsval_t missing_replacement;
	jsval_t result;
	generated_regex_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 8, &root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(named regex replace)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(named regex replace)");
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"(?<digits>[0-9])(?<tail>[a-z])?",
			sizeof("(?<digits>[0-9])(?<tail>[a-z])?") - 1,
			&pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(named regex replace args)");
	}
	if (jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&single_regex, &error) < 0
			|| jsval_regexp_new(&region, pattern, 1, global_flags,
				&global_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(named regex replace) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_string_new_utf8(&region,
			(const uint8_t *)"<$<digits>|$<tail>|$1>",
			sizeof("<$<digits>|$<tail>|$1>") - 1, &replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(named regex replacement)");
	}
	if (jsval_method_string_replace(&region, text, single_regex, replacement,
			&result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace(named regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"a<1|b|1>2",
			sizeof("a<1|b|1>2") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_method_string_replace_all(&region, text, global_regex,
			replacement, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all(named regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"a<1|b|1><2||2>",
			sizeof("a<1|b|1><2||2>") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"$<missing>",
			sizeof("$<missing>") - 1, &missing_replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(missing named replacement)");
	}
	if (jsval_method_string_replace(&region, text, single_regex,
			missing_replacement, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace(missing named regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"a$<missing>2",
			sizeof("a$<missing>2") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_method_string_replace_fn(&region, text, global_regex,
			generated_named_groups_replace_callback, &ctx,
			&result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_fn(named regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 named regex callback calls, got %d",
				ctx.call_count);
	}
	status = generated_expect_string(&region, result,
			(const uint8_t *)"a<1|b|1><2|U|3>",
			sizeof("a<1|b|1><2|U|3>") - 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	ctx.call_count = 0;
	if (jsval_method_string_replace_all_fn(&region, text, global_regex,
			generated_named_groups_replace_callback, &ctx,
			&result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_fn(named regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 named regex replaceAll callback calls, got %d",
				ctx.call_count);
	}
	return generated_expect_string(&region, result,
			(const uint8_t *)"a<1|b|1><2|U|3>",
			sizeof("a<1|b|1><2|U|3>") - 1, detail, cap);
}
#endif

static generated_status_t generated_smoke_jsval_method_replace_callback_abrupt(
		char *detail, size_t cap)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t text;
	jsval_t search;
	jsval_t result;
	generated_replace_callback_ctx_t ctx = {0, 1};
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, (const uint8_t *)"a1", 2, &text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&search) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(replace callback abrupt)");
	}
	if (jsval_method_string_replace_fn(&region, text, search,
			generated_replace_offset_callback, &ctx, &result, &error) == 0) {
		return generated_failf(detail, cap,
				"expected replace callback failure");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT callback failure, got %d",
				(int)error.kind);
	}
	if (ctx.call_count != 1) {
		return generated_failf(detail, cap,
				"expected 1 callback call before failure, got %d",
				ctx.call_count);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsmethod_replace_abrupt(
		char *detail, size_t cap)
{
	static const uint16_t x_units[] = {'X'};
	generated_callback_ctx_t throw_ctx = {1};
	int replacement_calls = 0;
	generated_callback_ctx_t later_replacement_ctx = {0, x_units,
			sizeof(x_units) / sizeof(x_units[0]), &replacement_calls};
	uint16_t storage[32];
	jsstr16_t out;
	jsmethod_error_t error;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_replace(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3),
			jsmethod_value_string_utf8((const uint8_t *)"z", 1),
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt replacement coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT replacement coercion, got %d",
				(int)error.kind);
	}
	if (jsmethod_string_replace(&out,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			jsmethod_value_string_utf8((const uint8_t *)"b", 1),
			jsmethod_value_coercible(&later_replacement_ctx,
					generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT receiver coercion, got %d",
				(int)error.kind);
	}
	if (replacement_calls != 0) {
		return generated_failf(detail, cap,
				"expected later replacement coercion not to run, got %d calls",
				replacement_calls);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsmethod_replace_all_abrupt(
		char *detail, size_t cap)
{
	static const uint16_t x_units[] = {'X'};
	generated_callback_ctx_t throw_ctx = {1};
	int replacement_calls = 0;
	generated_callback_ctx_t later_replacement_ctx = {0, x_units,
			sizeof(x_units) / sizeof(x_units[0]), &replacement_calls};
	uint16_t storage[32];
	jsstr16_t out;
	jsmethod_error_t error;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_replace_all(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3),
			jsmethod_value_string_utf8((const uint8_t *)"z", 1),
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt replaceAll coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT replaceAll coercion, got %d",
				(int)error.kind);
	}
	if (jsmethod_string_replace_all(&out,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			jsmethod_value_string_utf8((const uint8_t *)"b", 1),
			jsmethod_value_coercible(&later_replacement_ctx,
					generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt replaceAll receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT replaceAll receiver coercion, got %d",
				(int)error.kind);
	}
	if (replacement_calls != 0) {
		return generated_failf(detail, cap,
				"expected later replaceAll coercion not to run, got %d calls",
				replacement_calls);
	}
	return GENERATED_PASS;
}

#if JSMX_WITH_REGEX
static generated_status_t generated_smoke_jsval_method_regex_replace(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"a1b2\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b2\",\"regex\":\"a<1>b<2>\",\"first\":\"a[1][1]b2\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t global_regex;
	jsval_t single_regex;
	jsval_t global_replacement;
	jsval_t single_replacement;
	jsval_t regex_result;
	jsval_t first_result;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;
	size_t last_index;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 8, &root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(regex replace)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(regex replace text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
			&pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"<$1>", 4,
			&global_replacement) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"[$&][$1]", 8,
			&single_replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex replace args)");
	}
	if (jsval_regexp_new(&region, pattern, 1, global_flags, &global_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(global replace) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_new(&region, pattern, 0, jsval_undefined(), &single_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(single replace) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_set_last_index(&region, global_regex, 1) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_set_last_index(global replace)");
	}
	if (jsval_method_string_replace_measure(&region, text, global_regex,
			global_replacement, &sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_measure(regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 8) {
		return generated_failf(detail, cap,
				"expected regex replace result_len 8, got %zu",
				sizes.result_len);
	}
	if (jsval_method_string_replace(&region, text, global_regex,
			global_replacement, &regex_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace(regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, regex_result,
			(const uint8_t *)"a<1>b<2>", 8, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_get_last_index(&region, global_regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(global replace)");
	}
	if (last_index != 1) {
		return generated_failf(detail, cap,
				"expected preserved lastIndex 1, got %zu", last_index);
	}
	if (jsval_method_string_replace(&region, text, single_regex,
			single_replacement, &first_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace(single regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, first_result,
			(const uint8_t *)"a[1][1]b2", 9, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(regex replace)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"regex", 5,
			regex_result) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"first", 5, first_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex replace)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_regex_replace_all(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"a1b2\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b2\",\"regex\":\"a<1>b<2>\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t global_regex;
	jsval_t single_regex;
	jsval_t replacement;
	jsval_t regex_result;
	jsmethod_error_t error;
	jsmethod_string_replace_sizes_t sizes;
	size_t last_index;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 8, &root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(regex replaceAll)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(regex replaceAll text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
			&pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"<$1>", 4,
			&replacement) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex replaceAll args)");
	}
	if (jsval_regexp_new(&region, pattern, 1, global_flags, &global_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(global replaceAll) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&single_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(single replaceAll) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_set_last_index(&region, global_regex, 1) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_set_last_index(global replaceAll)");
	}
	if (jsval_method_string_replace_all_measure(&region, text, global_regex,
			replacement, &sizes, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_measure(regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 8) {
		return generated_failf(detail, cap,
				"expected regex replaceAll result_len 8, got %zu",
				sizes.result_len);
	}
	if (jsval_method_string_replace_all(&region, text, global_regex,
			replacement, &regex_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all(regex) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, regex_result,
			(const uint8_t *)"a<1>b<2>", 8, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_get_last_index(&region, global_regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(global replaceAll)");
	}
	if (last_index != 1) {
		return generated_failf(detail, cap,
				"expected preserved lastIndex 1, got %zu", last_index);
	}
	if (jsval_method_string_replace_all_measure(&region, text, single_regex,
			replacement, &sizes, &error) == 0) {
		return generated_failf(detail, cap,
				"expected non-global replaceAll measure to fail");
	}
	if (error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_failf(detail, cap,
				"expected TYPE for non-global replaceAll measure, got %d",
				(int)error.kind);
	}
	if (jsval_method_string_replace_all(&region, text, single_regex,
			replacement, &regex_result, &error) == 0) {
		return generated_failf(detail, cap,
				"expected non-global replaceAll to fail");
	}
	if (error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_failf(detail, cap,
				"expected TYPE for non-global replaceAll, got %d",
				(int)error.kind);
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 2) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(regex replaceAll)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"regex", 5,
			regex_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex replaceAll)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_regexp_exec_match(
		char *detail, size_t cap)
{
	static const uint8_t input_json[] = "{\"text\":\"343443444\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"343443444\",\"exec0\":\"12b\",\"matchCount\":3}";
	static const uint8_t exec_pattern_utf8[] = "([0-9][0-9])([a-z])?";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t exec_pattern;
	jsval_t match_pattern;
	jsval_t match_flags;
	jsval_t exec_regex;
	jsval_t match_regex;
	jsval_t exec_subject;
	jsval_t exec_result;
	jsval_t match_result;
	jsval_t capture0;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input_json, sizeof(input_json) - 1, 8,
			&root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(regex root)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}
	if (jsval_string_new_utf8(&region, exec_pattern_utf8,
			sizeof(exec_pattern_utf8) - 1, &exec_pattern) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(exec_pattern)");
	}
	if (jsval_regexp_new(&region, exec_pattern, 0, jsval_undefined(),
			&exec_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(exec) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"a12b", 4,
			&exec_subject) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(exec_subject)");
	}
	if (jsval_regexp_exec(&region, exec_regex, exec_subject, &exec_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (exec_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap, "expected object-like exec result");
	}
	if (jsval_object_get_utf8(&region, exec_result, (const uint8_t *)"0", 1,
			&capture0) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec capture)");
	}
	status = generated_expect_string(&region, capture0,
			(const uint8_t *)"12b", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_string_new_utf8(&region, (const uint8_t *)"3", 1,
			&match_pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&match_flags) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(match args)");
	}
	if (jsval_regexp_new(&region, match_pattern, 1, match_flags, &match_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(match) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_match(&region, text, 1, match_regex, &match_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (match_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, match_result) != 3) {
		return generated_failf(detail, cap,
				"expected 3-entry global match result");
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 3) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(regex)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root(regex)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"exec0", 5,
			capture0) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"matchCount", 10,
				jsval_number((double)jsval_array_length(&region, match_result))) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex result)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_regexp_exec_state(
		char *detail, size_t cap)
{
	static const uint8_t input_json[] = "{\"text\":\"a1b2\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b2\",\"first\":\"1\",\"second\":\"2\",\"miss\":true,\"lastIndex\":0,\"nonGlobalLastIndex\":7}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t global_regex;
	jsval_t non_global_regex;
	jsval_t first_result;
	jsval_t second_result;
	jsval_t miss_result;
	jsval_t nonglobal_result;
	jsval_t first_match;
	jsval_t second_match;
	size_t last_index = 0;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input_json, sizeof(input_json) - 1, 8,
			&root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(regex exec state)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(regex exec state)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
			&pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
			&global_flags) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex exec state args)");
	}
	if (jsval_regexp_new(&region, pattern, 1, global_flags, &global_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(global exec state) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&non_global_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(non-global exec state) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_exec(&region, global_regex, text, &first_result, &error) < 0) {
		return generated_failf(detail, cap,
				"first jsval_regexp_exec failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, first_result, (const uint8_t *)"0", 1,
			&first_match) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(first exec match)");
	}
	status = generated_expect_string(&region, first_match,
			(const uint8_t *)"1", 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_exec(&region, global_regex, text, &second_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"second jsval_regexp_exec failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, second_result, (const uint8_t *)"0", 1,
			&second_match) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(second exec match)");
	}
	status = generated_expect_string(&region, second_match,
			(const uint8_t *)"2", 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_exec(&region, global_regex, text, &miss_result, &error) < 0) {
		return generated_failf(detail, cap,
				"third jsval_regexp_exec failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (miss_result.kind != JSVAL_KIND_NULL) {
		return generated_failf(detail, cap,
				"expected null after global miss");
	}
	if (jsval_regexp_get_last_index(&region, global_regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(global exec state)");
	}
	if (last_index != 0) {
		return generated_failf(detail, cap,
				"expected reset lastIndex 0, got %zu", last_index);
	}
	if (jsval_regexp_set_last_index(&region, non_global_regex, 7) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_set_last_index(non-global exec state)");
	}
	if (jsval_regexp_exec(&region, non_global_regex, text, &nonglobal_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"non-global jsval_regexp_exec failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_get_last_index(&region, non_global_regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(non-global exec state)");
	}
	if (last_index != 7) {
		return generated_failf(detail, cap,
				"expected preserved non-global lastIndex 7, got %zu",
				last_index);
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 6) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(exec state)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_region_set_root(exec state)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"first", 5,
			first_match) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"second", 6,
				second_match) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"miss", 4,
				jsval_bool(1)) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"lastIndex", 9,
				jsval_number(0.0)) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"nonGlobalLastIndex", 18,
				jsval_number((double)last_index)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(exec state result)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_regexp_core(
		char *detail, size_t cap)
{
	static const uint8_t input_json[] = "{\"text\":\"abc\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"abc\",\"source\":\"abc\",\"copyFlags\":\"gim\",\"overrideFlags\":\"y\",\"global\":true,\"ignoreCase\":true,\"multiline\":true,\"dotAll\":false,\"unicode\":false,\"sticky\":true,\"test\":true,\"lastIndex\":3}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t gim_flags;
	jsval_t y_flag;
	jsval_t base_regex;
	jsval_t copy_regex;
	jsval_t override_regex;
	jsval_t flags_value;
	jsval_t source_value;
	jsval_regexp_info_t info;
	jsmethod_error_t error;
	int test_result;
	int global_value;
	int ignore_case_value;
	int multiline_value;
	int dot_all_value;
	int unicode_value;
	int sticky_value;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input_json, sizeof(input_json) - 1, 8,
			&root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse(regex core)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(regex core text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"abc", 3, &pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"gim", 3,
				&gim_flags) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"y", 1,
				&y_flag) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex core args)");
	}
	if (jsval_regexp_new(&region, pattern, 1, gim_flags, &base_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(base) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_set_last_index(&region, base_regex, 9) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_set_last_index(base)");
	}
	if (jsval_regexp_new(&region, base_regex, 0, jsval_undefined(),
			&copy_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(copy) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_source(&region, copy_regex, &source_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_regexp_source(copy)");
	}
	status = generated_expect_string(&region, source_value,
			(const uint8_t *)"abc", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_flags(&region, copy_regex, &flags_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_regexp_flags(copy)");
	}
	status = generated_expect_string(&region, flags_value,
			(const uint8_t *)"gim", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_new(&region, base_regex, 1, y_flag, &override_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(override) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_flags(&region, override_regex, &flags_value) < 0) {
		return generated_fail_errno(detail, cap, "jsval_regexp_flags(override)");
	}
	status = generated_expect_string(&region, flags_value,
			(const uint8_t *)"y", 1, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_regexp_set_last_index(&region, override_regex, 0) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_set_last_index(override)");
	}
	if (jsval_regexp_test(&region, override_regex, text, &test_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_test failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (generated_expect_boolean_result(test_result, 1, detail, cap)
			!= GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_regexp_info(&region, override_regex, &info) < 0) {
		return generated_fail_errno(detail, cap, "jsval_regexp_info");
	}
	if (info.last_index != 3) {
		return generated_failf(detail, cap,
				"expected lastIndex 3 after sticky test, got %zu",
				info.last_index);
	}
	if (jsval_regexp_global(&region, copy_regex, &global_value) < 0
			|| jsval_regexp_ignore_case(&region, copy_regex, &ignore_case_value) < 0
			|| jsval_regexp_multiline(&region, copy_regex, &multiline_value) < 0
			|| jsval_regexp_dot_all(&region, copy_regex, &dot_all_value) < 0
			|| jsval_regexp_unicode(&region, copy_regex, &unicode_value) < 0
			|| jsval_regexp_sticky(&region, override_regex, &sticky_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_* accessor");
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 12) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(regex core)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_region_set_root(regex core)");
	}
	if (jsval_regexp_flags(&region, copy_regex, &flags_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_flags(copy write)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"source", 6,
			source_value) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"copyFlags", 9, flags_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex core copy)");
	}
	if (jsval_regexp_flags(&region, override_regex, &flags_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_flags(override write)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"overrideFlags", 13,
			flags_value) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"global", 6,
				jsval_bool(global_value)) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"ignoreCase", 10,
				jsval_bool(ignore_case_value)) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"multiline", 9,
				jsval_bool(multiline_value)) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"dotAll", 6,
				jsval_bool(dot_all_value)) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"unicode", 7,
				jsval_bool(unicode_value)) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"sticky", 6,
				jsval_bool(sticky_value)) < 0
			|| jsval_object_set_utf8(&region, root, (const uint8_t *)"test", 4,
				jsval_bool(test_result)) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"lastIndex", 9,
				jsval_number((double)info.last_index)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(regex core write)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_regexp_match_all(
		char *detail, size_t cap)
{
	static const uint8_t input_json[] = "{\"text\":\"a1b2\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"a1b2\",\"first\":\"1\",\"second\":\"2\",\"count\":2,\"lastIndex\":1}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t iterator;
	jsval_t result;
	jsval_t first;
	jsval_t second;
	jsmethod_error_t error;
	size_t last_index = 0;
	int done = 0;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input_json, sizeof(input_json) - 1, 8,
			&root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_json_parse(regex matchAll)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(regex matchAll text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"([0-9])", 7,
			&pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(regex matchAll args)");
	}
	if (jsval_regexp_new(&region, pattern, 1, global_flags, &regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(matchAll) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_set_last_index(&region, regex, 1) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_set_last_index(matchAll)");
	}
	if (jsval_method_string_match_all(&region, text, 1, regex, &iterator,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (iterator.kind != JSVAL_KIND_MATCH_ITERATOR) {
		return generated_failf(detail, cap,
				"expected matchAll iterator result");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first matchAll result object");
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"1", 1,
			&first) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(matchAll first capture)");
	}
	status = generated_expect_string(&region, first, (const uint8_t *)"1", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected second matchAll result object");
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"1", 1,
			&second) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(matchAll second capture)");
	}
	status = generated_expect_string(&region, second, (const uint8_t *)"2", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected matchAll iterator exhaustion");
	}
	if (jsval_regexp_get_last_index(&region, regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(matchAll)");
	}
	if (last_index != 1) {
		return generated_failf(detail, cap,
				"expected original regex lastIndex 1, got %zu", last_index);
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 5) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place(matchAll)");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_region_set_root(matchAll)");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"first", 5,
			first) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"second", 6, second) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"count", 5, jsval_number(2.0)) < 0
			|| jsval_object_set_utf8(&region, root,
				(const uint8_t *)"lastIndex", 9,
				jsval_number((double)last_index)) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(matchAll result)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_regexp_named_groups(
		char *detail, size_t cap)
{
	static const uint8_t pattern_utf8[] = "(?<digits>[0-9])(?<tail>[a-z])?";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t global_regex;
	jsval_t subject;
	jsval_t subject_without_tail;
	jsval_t iterator;
	jsval_t result;
	jsval_t groups;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region,
			pattern_utf8, sizeof(pattern_utf8) - 1, &pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
				&subject) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"a2", 2,
				&subject_without_tail) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(named groups args)");
	}
	if (jsval_regexp_new(&region, pattern, 0, jsval_undefined(), &regex,
			&error) < 0
			|| jsval_regexp_new(&region, pattern, 1, global_flags,
				&global_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(named groups) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	if (jsval_regexp_exec(&region, regex, subject, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec(named groups) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"groups", 6,
			&groups) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec groups)");
	}
	if (groups.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected exec groups object");
	}
	if (jsval_object_get_utf8(&region, groups, (const uint8_t *)"digits", 6,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec groups.digits)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"1", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_object_get_utf8(&region, groups, (const uint8_t *)"tail", 4,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec groups.tail)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"b", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}

	if (jsval_regexp_exec(&region, regex, subject_without_tail, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec(named groups optional) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"groups", 6,
			&groups) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec optional groups)");
	}
	if (jsval_object_get_utf8(&region, groups, (const uint8_t *)"tail", 4,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec optional groups.tail)");
	}
	if (result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected optional named group to be undefined");
	}

	if (jsval_method_string_match(&region, subject, 1, regex, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match(named groups) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"groups", 6,
			&groups) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(match groups)");
	}
	if (jsval_object_get_utf8(&region, groups, (const uint8_t *)"digits", 6,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(match groups.digits)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"1", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}

	if (jsval_method_string_match_all(&region, subject, 1, global_regex,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all(named groups) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) < 0 || done) {
		return generated_failf(detail, cap,
				"first named-group matchAll result failed: errno=%d kind=%d done=%d",
				errno, (int)error.kind, done);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"groups", 6,
			&groups) < 0
			|| jsval_object_get_utf8(&region, groups,
				(const uint8_t *)"tail", 4, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"matchAll first groups lookup");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"b", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) < 0 || done) {
		return generated_failf(detail, cap,
				"second named-group matchAll result failed: errno=%d kind=%d done=%d",
				errno, (int)error.kind, done);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"groups", 6,
			&groups) < 0
			|| jsval_object_get_utf8(&region, groups,
				(const uint8_t *)"digits", 6, &result) < 0) {
		return generated_fail_errno(detail, cap,
				"matchAll second groups lookup");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"2", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_object_get_utf8(&region, groups, (const uint8_t *)"tail", 4,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"matchAll second groups.tail lookup");
	}
	if (result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected second matchAll optional named group to be undefined");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"named-group matchAll exhaustion failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected named-group matchAll iterator exhaustion");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_regexp_named_groups_jit(
		char *detail, size_t cap)
{
	static const uint8_t pattern_utf8[] = "(?<digits>[0-9])(?<tail>[a-z])?";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t regex;
	jsval_t subject;
	jsval_t result;
	jsval_t groups;
	size_t last_index = 0;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, pattern_utf8,
			sizeof(pattern_utf8) - 1, &pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
				&subject) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(named groups jit args)");
	}
	if (jsval_regexp_new_jit(&region, pattern, 1, global_flags, &regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new_jit(named groups) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_exec(&region, regex, subject, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec(named groups jit) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"groups", 6,
			&groups) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec jit groups)");
	}
	if (jsval_object_get_utf8(&region, groups, (const uint8_t *)"digits", 6,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(exec jit groups.digits)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"1", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_regexp_get_last_index(&region, regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(jit)");
	}
	if (last_index != 3) {
		return generated_failf(detail, cap,
				"expected jit lastIndex 3, got %zu", last_index);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_regexp_constructor_choice(
		char *detail, size_t cap)
{
	static const uint8_t pattern_utf8[] = "([0-9])";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t one_shot_regex;
	jsval_t reused_regex;
	jsval_t subject;
	jsval_t result;
	size_t last_index = 0;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf8(&region, pattern_utf8,
			sizeof(pattern_utf8) - 1, &pattern) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"g", 1,
				&global_flags) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"a1b2", 4,
				&subject) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(constructor choice args)");
	}
	if (jsval_regexp_new(&region, pattern, 0, jsval_undefined(),
			&one_shot_regex, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new(one-shot) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_exec(&region, one_shot_regex, subject, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec(one-shot) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"0", 1,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(one-shot match)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"1", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_regexp_get_last_index(&region, one_shot_regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(one-shot)");
	}
	if (last_index != 0) {
		return generated_failf(detail, cap,
				"expected one-shot lastIndex 0, got %zu", last_index);
	}

	if (jsval_regexp_new_jit(&region, pattern, 1, global_flags, &reused_regex,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_new_jit(reused) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_regexp_exec(&region, reused_regex, subject, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec(reused first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"0", 1,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(reused first match)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"1", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_regexp_exec(&region, reused_regex, subject, &result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_regexp_exec(reused second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_get_utf8(&region, result, (const uint8_t *)"0", 1,
			&result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(reused second match)");
	}
	if (generated_expect_string(&region, result, (const uint8_t *)"2", 1,
			detail, cap) != GENERATED_PASS) {
		return GENERATED_WRONG_RESULT;
	}
	if (jsval_regexp_get_last_index(&region, reused_regex, &last_index) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_regexp_get_last_index(reused)");
	}
	if (last_index != 4) {
		return generated_failf(detail, cap,
				"expected reused lastIndex 4, got %zu", last_index);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_u_literal_surrogate_rewrite(
		char *detail, size_t cap)
{
	static const uint16_t subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'C'
	};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t replace_expected[] = {
		'A', 0xD834, 0xDF06, 'X', 'B', 0xDF06, 'C'
	};
	static const uint16_t callback_expected[] = {
		'A', 0xD834, 0xDF06, '[', '3', ']', 'B', '[', '5', ']', 'C'
	};
	static const uint16_t prefix_expected[] = {'A', 0xD834, 0xDF06};
	static const uint16_t b_unit[] = {'B'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t text;
	jsval_t replacement;
	jsval_t limit_two;
	jsval_t match_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t replace_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, subject_units,
			sizeof(subject_units) / sizeof(subject_units[0]), &text) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16(surrogate rewrite text)");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"X", 1,
			&replacement) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf8(surrogate rewrite args)");
	}
	if (jsval_method_string_match_u_literal_surrogate(&region, text, 0xDF06, 1,
			&match_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_u_literal_surrogate failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (match_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, match_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry surrogate match array");
	}
	if (jsval_array_get(&region, match_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(surrogate match 0)");
	}
	status = generated_expect_utf16_string(&region, value, low_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, match_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(surrogate match 1)");
	}
	status = generated_expect_utf16_string(&region, value, low_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_match_all_u_literal_surrogate(&region, text,
			0xDF06, &iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_literal_surrogate failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(surrogate first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first surrogate matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(surrogate matchAll first capture)");
	}
	status = generated_expect_utf16_string(&region, value, low_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected first surrogate matchAll index 3");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(surrogate second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected second surrogate matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(surrogate matchAll second capture)");
	}
	status = generated_expect_utf16_string(&region, value, low_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(5.0)) != 1) {
		return generated_failf(detail, cap,
				"expected second surrogate matchAll index 5");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(surrogate done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || iterator_result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected surrogate matchAll iterator exhaustion");
	}

	if (jsval_method_string_replace_u_literal_surrogate(&region, text, 0xDF06,
			replacement, &replace_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_u_literal_surrogate failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_utf16_string(&region, replace_result,
			replace_expected,
			sizeof(replace_expected) / sizeof(replace_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_replace_all_u_literal_surrogate_fn(&region, text,
			0xDF06, generated_replace_offset_callback, &ctx, &callback_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_literal_surrogate_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 surrogate callback calls, got %d", ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_literal_surrogate(&region, text, 0xDF06, 1,
			limit_two, &split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_literal_surrogate failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry surrogate split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(surrogate split 0)");
	}
	status = generated_expect_utf16_string(&region, value, prefix_expected,
			sizeof(prefix_expected) / sizeof(prefix_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(surrogate split 1)");
	}
	return generated_expect_utf16_string(&region, value, b_unit, 1,
			detail, cap);
}

static generated_status_t
generated_smoke_jsval_u_literal_sequence_rewrite(char *detail, size_t cap)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'B', 'C'
	};
	static const uint16_t pair_subject_units[] = {
		'A', 0xD834, 0xDF06, '!', 'X', 0xD834, 0xDF06, '!', 'Y'
	};
	static const uint16_t low_b_units[] = {0xDF06, 'B'};
	static const uint16_t pair_bang_units[] = {0xD834, 0xDF06, '!'};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t callback_expected[] = {
		'A', 0xD834, 0xDF06, '[', '3', ']', '[', '5', ']', 'C'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_text;
	jsval_t pair_text;
	jsval_t limit_two;
	jsval_t search_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_text) < 0
			|| jsval_string_new_utf16(&region, pair_subject_units,
				sizeof(pair_subject_units) / sizeof(pair_subject_units[0]),
				&pair_text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16/utf8(sequence rewrite args)");
	}

	if (jsval_method_string_search_u_literal_sequence(&region, pair_text,
			pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]),
			&search_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_u_literal_sequence failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_strict_eq(&region, search_result, jsval_number(1.0)) != 1) {
		return generated_failf(detail, cap,
				"expected pair sequence search index 1");
	}

	if (jsval_method_string_match_all_u_literal_sequence(&region, low_text,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]),
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_literal_sequence failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(sequence first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first sequence matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(sequence matchAll first capture)");
	}
	status = generated_expect_utf16_string(&region, value, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected first sequence matchAll index 3");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(sequence second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected second sequence matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(sequence matchAll second capture)");
	}
	status = generated_expect_utf16_string(&region, value, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(5.0)) != 1) {
		return generated_failf(detail, cap,
				"expected second sequence matchAll index 5");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(sequence done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || iterator_result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected sequence matchAll iterator exhaustion");
	}

	if (jsval_method_string_replace_all_u_literal_sequence_fn(&region, low_text,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]),
			generated_replace_offset_callback, &ctx, &callback_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_literal_sequence_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 sequence callback calls, got %d", ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_literal_sequence(&region, low_text,
			low_b_units, sizeof(low_b_units) / sizeof(low_b_units[0]), 1,
			limit_two, &split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_literal_sequence failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry sequence split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(sequence split 0)");
	}
	status = generated_expect_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(sequence split 1)");
	}
	status = generated_expect_utf16_string(&region, value, NULL, 0,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_match_u_literal_sequence(&region, pair_text,
			pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]), 0, &value,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_u_literal_sequence failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (value.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected non-global sequence match object");
	}
	if (jsval_object_get_utf8(&region, value, (const uint8_t *)"0", 1,
			&iterator_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(sequence match capture)");
	}
	return generated_expect_utf16_string(&region, iterator_result,
			pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]),
			detail, cap);
}

static generated_status_t
generated_smoke_jsval_u_literal_class_rewrite(char *detail, size_t cap)
{
	static const uint16_t class_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 'D', 0xD834, 'C'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_members[] = {0xD834};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t callback_expected[] = {
		'A', 0xD834, 0xDF06, '[', '3', ']', 'B', '[', '5', ']',
		0xD834, 'C'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_text;
	jsval_t high_text;
	jsval_t limit_two;
	jsval_t search_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_text) < 0
			|| jsval_string_new_utf16(&region, high_subject_units,
				sizeof(high_subject_units) /
				sizeof(high_subject_units[0]), &high_text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16/utf8(class rewrite args)");
	}

	if (jsval_method_string_search_u_literal_class(&region, class_text,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			&search_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_u_literal_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_strict_eq(&region, search_result, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected class search index 3");
	}

	if (jsval_method_string_match_all_u_literal_class(&region, class_text,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_literal_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(class first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first class matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(class matchAll first capture)");
	}
	status = generated_expect_utf16_string(&region, value, low_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected first class matchAll index 3");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(class second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected second class matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(class matchAll second capture)");
	}
	status = generated_expect_utf16_string(&region, value, d_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(5.0)) != 1) {
		return generated_failf(detail, cap,
				"expected second class matchAll index 5");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(class done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || iterator_result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected class matchAll iterator exhaustion");
	}

	if (jsval_method_string_replace_all_u_literal_class_fn(&region,
			class_text, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			generated_replace_offset_callback, &ctx, &callback_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_literal_class_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 class callback calls, got %d", ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_literal_class(&region, class_text,
			low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1,
			limit_two, &split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_literal_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry class split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(class split 0)");
	}
	status = generated_expect_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(class split 1)");
	}
	status = generated_expect_utf16_string(&region, value, b_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_match_u_literal_class(&region, high_text,
			high_members, sizeof(high_members) / sizeof(high_members[0]), 0,
			&value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_u_literal_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (value.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected non-global class match object");
	}
	if (jsval_object_get_utf8(&region, value, (const uint8_t *)"0", 1,
			&iterator_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(class match capture)");
	}
	return generated_expect_utf16_string(&region, iterator_result,
			high_members, sizeof(high_members) / sizeof(high_members[0]),
			detail, cap);
}

static generated_status_t generated_smoke_jsval_method_regex_search(
		char *detail, size_t cap)
{
	static const uint8_t input[] =
		"{\"text\":\"fooBar\",\"pattern\":\"bar\",\"flags\":\"i\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"fooBar\",\"pattern\":\"bar\",\"flags\":\"i\",\"search\":3}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t pattern;
	jsval_t flags;
	jsval_t result;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0 ||
			jsval_object_get_utf8(&region, root, (const uint8_t *)"pattern", 7,
			&pattern) < 0 ||
			jsval_object_get_utf8(&region, root, (const uint8_t *)"flags", 5,
			&flags) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(regex)");
	}
	if (jsval_method_string_search_regex(&region, text, pattern, 1, flags,
			&result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_regex failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_number(&region, result, 3.0, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 4) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"search", 6,
			result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(search)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_regex_search_abrupt(
		char *detail, size_t cap)
{
	static const uint8_t subject_utf8[] = "fooBar";
	static const uint16_t flags_i[] = {'i'};
	generated_callback_ctx_t throw_ctx = {1};
	int flag_calls = 0;
	generated_callback_ctx_t flags_ctx = {0, flags_i, 1, &flag_calls};
	jsmethod_error_t error;
	ssize_t index;

	if (jsmethod_string_search_regex(&index,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			jsmethod_value_string_utf8((const uint8_t *)"bar", 3),
			1, jsmethod_value_coercible(&flags_ctx, generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt regex receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT regex receiver coercion, got %d",
				(int)error.kind);
	}
	if (flag_calls != 0) {
		return generated_failf(detail, cap,
				"expected later regex flags coercion not to run");
	}

	if (jsmethod_string_search_regex(&index,
			jsmethod_value_string_utf8(subject_utf8,
				sizeof(subject_utf8) - 1),
			jsmethod_value_string_utf8((const uint8_t *)"[", 1),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_failf(detail, cap,
				"expected invalid regular expression to fail");
	}
	if (error.kind != JSMETHOD_ERROR_SYNTAX) {
		return generated_failf(detail, cap,
				"expected SYNTAX regex error, got %d",
				(int)error.kind);
	}

	return GENERATED_PASS;
}

static generated_status_t
generated_smoke_jsval_u_literal_negated_class_rewrite(char *detail,
		size_t cap)
{
	static const uint16_t class_subject_units[] = {
		0xD834, 0xDF06, 0xDF06, 'B', 'D'
	};
	static const uint16_t high_subject_units[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_c_members[] = {0xD834, 'C'};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t pair_low_units[] = {0xD834, 0xDF06, 0xDF06};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, 0xDF06, '[', '3', ']', 'D'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_text;
	jsval_t high_text;
	jsval_t limit_two;
	jsval_t search_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_text) < 0
			|| jsval_string_new_utf16(&region, high_subject_units,
				sizeof(high_subject_units) /
				sizeof(high_subject_units[0]), &high_text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16/utf8(negated class rewrite args)");
	}

	if (jsval_method_string_search_u_literal_negated_class(&region,
			class_text, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			&search_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_u_literal_negated_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_strict_eq(&region, search_result, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected negated class search index 3");
	}

	if (jsval_method_string_match_all_u_literal_negated_class(&region,
			class_text, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &iterator,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_literal_negated_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(negated class first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first negated class matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(negated class matchAll capture)");
	}
	status = generated_expect_utf16_string(&region, value, b_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected negated class matchAll index 3");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(negated class done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || iterator_result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected negated class matchAll iterator exhaustion");
	}

	if (jsval_method_string_replace_all_u_literal_negated_class_fn(&region,
			class_text, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]),
			generated_replace_offset_callback, &ctx, &callback_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_literal_negated_class_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 1) {
		return generated_failf(detail, cap,
				"expected 1 negated class callback call, got %d",
				ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_literal_negated_class(&region,
			class_text, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1,
			limit_two, &split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_literal_negated_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry negated class split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(negated class split 0)");
	}
	status = generated_expect_utf16_string(&region, value, pair_low_units,
			sizeof(pair_low_units) / sizeof(pair_low_units[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(negated class split 1)");
	}
	status = generated_expect_utf16_string(&region, value, d_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_match_u_literal_negated_class(&region, high_text,
			high_c_members, sizeof(high_c_members) /
			sizeof(high_c_members[0]), 0, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_u_literal_negated_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (value.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected non-global negated class match object");
	}
	if (jsval_object_get_utf8(&region, value, (const uint8_t *)"0", 1,
			&iterator_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(negated class match capture)");
	}
	return generated_expect_utf16_string(&region, iterator_result, b_unit, 1,
			detail, cap);
}

static generated_status_t
generated_smoke_jsval_u_literal_range_class_rewrite(char *detail, size_t cap)
{
	static const uint16_t class_subject_units[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_range[] = {0xD834, 0xD834};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t callback_expected[] = {
		'A', 0xD834, 0xDF06, '[', '3', ']', '[', '4', ']',
		'[', '5', ']', 'Z'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_text;
	jsval_t high_text;
	jsval_t limit_two;
	jsval_t search_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_text) < 0
			|| jsval_string_new_utf16(&region, high_subject_units,
				sizeof(high_subject_units) /
				sizeof(high_subject_units[0]), &high_text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16/utf8(range rewrite args)");
	}

	if (jsval_method_string_search_u_literal_range_class(&region, class_text,
			b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &search_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_u_literal_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_strict_eq(&region, search_result, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected range class search index 3");
	}

	if (jsval_method_string_match_all_u_literal_range_class(&region,
			class_text, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &iterator,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_literal_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(range class first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first range class matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(range class first capture)");
	}
	status = generated_expect_utf16_string(&region, value, b_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected first range class matchAll index 3");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(range class second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected second range class matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(range class second capture)");
	}
	status = generated_expect_utf16_string(&region, value, d_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(range class third) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected third range class matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(range class third capture)");
	}
	status = generated_expect_utf16_string(&region, value, low_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(5.0)) != 1) {
		return generated_failf(detail, cap,
				"expected third range class matchAll index 5");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(range class done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || iterator_result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected range class matchAll iterator exhaustion");
	}

	if (jsval_method_string_replace_all_u_literal_range_class_fn(&region,
			class_text, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])),
			generated_replace_offset_callback, &ctx, &callback_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_literal_range_class_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 3) {
		return generated_failf(detail, cap,
				"expected 3 range class callback calls, got %d",
				ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_literal_range_class(&region, class_text,
			b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, limit_two,
			&split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_literal_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry range class split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(range class split 0)");
	}
	status = generated_expect_utf16_string(&region, value, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(range class split 1)");
	}
	status = generated_expect_utf16_string(&region, value, NULL, 0,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_match_u_literal_range_class(&region, high_text,
			high_range, sizeof(high_range) / (2 * sizeof(high_range[0])), 0,
			&value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_u_literal_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (value.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected non-global range class match object");
	}
	if (jsval_object_get_utf8(&region, value, (const uint8_t *)"0", 1,
			&iterator_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(range class match capture)");
	}
	return generated_expect_utf16_string(&region, iterator_result,
			high_range, 1, detail, cap);
}

static generated_status_t
generated_smoke_jsval_u_literal_negated_range_class_rewrite(char *detail,
		size_t cap)
{
	static const uint16_t class_subject_units[] = {
		0xD834, 0xDF06, 'A', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t high_subject_units[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_and_c_ranges[] = {
		0xD834, 0xD834, 'C', 'C'
	};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t z_unit[] = {'Z'};
	static const uint16_t pair_units[] = {0xD834, 0xDF06};
	static const uint16_t b_d_low_units[] = {'B', 'D', 0xDF06};
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, '[', '2', ']', 'B', 'D', 0xDF06,
		'[', '6', ']'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_text;
	jsval_t high_text;
	jsval_t limit_two;
	jsval_t search_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_text) < 0
			|| jsval_string_new_utf16(&region, high_subject_units,
				sizeof(high_subject_units) /
				sizeof(high_subject_units[0]), &high_text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16/utf8(negated range args)");
	}

	if (jsval_method_string_search_u_literal_negated_range_class(&region,
			class_text, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &search_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_u_literal_negated_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_strict_eq(&region, search_result, jsval_number(2.0)) != 1) {
		return generated_failf(detail, cap,
				"expected negated range search index 2");
	}

	if (jsval_method_string_match_all_u_literal_negated_range_class(&region,
			class_text, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &iterator,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_literal_negated_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(negated range first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first negated range matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(negated range first capture)");
	}
	status = generated_expect_utf16_string(&region, value, a_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(negated range second) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected second negated range matchAll iterator result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(negated range second capture)");
	}
	status = generated_expect_utf16_string(&region, value, z_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"index", 5, &value) < 0
			|| jsval_strict_eq(&region, value, jsval_number(6.0)) != 1) {
		return generated_failf(detail, cap,
				"expected second negated range matchAll index 6");
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(negated range done) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (!done || iterator_result.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected negated range matchAll iterator exhaustion");
	}

	if (jsval_method_string_replace_all_u_literal_negated_range_class_fn(
			&region, class_text, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])),
			generated_replace_offset_callback, &ctx, &callback_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_literal_negated_range_class_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 2) {
		return generated_failf(detail, cap,
				"expected 2 negated range callback calls, got %d",
				ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_literal_negated_range_class(&region,
			class_text, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, limit_two,
			&split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_literal_negated_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry negated range split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(negated range split 0)");
	}
	status = generated_expect_utf16_string(&region, value, pair_units,
			sizeof(pair_units) / sizeof(pair_units[0]), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(negated range split 1)");
	}
	status = generated_expect_utf16_string(&region, value, b_d_low_units,
			sizeof(b_d_low_units) / sizeof(b_d_low_units[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_match_u_literal_negated_range_class(&region,
			high_text, high_and_c_ranges,
			sizeof(high_and_c_ranges) /
			(2 * sizeof(high_and_c_ranges[0])), 0, &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_u_literal_negated_range_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (value.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected non-global negated range match object");
	}
	if (jsval_object_get_utf8(&region, value, (const uint8_t *)"0", 1,
			&iterator_result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(negated range match capture)");
	}
	return generated_expect_utf16_string(&region, iterator_result, b_unit, 1,
			detail, cap);
}

static generated_status_t
generated_smoke_jsval_u_predefined_class_rewrite(char *detail, size_t cap)
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
	static const uint16_t callback_expected[] = {
		0xD834, 0xDF06, '[', '2', ']', 'A', '1', '_',
		'[', '6', ']', '[', '7', ']'
	};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t digit_text;
	jsval_t whitespace_text;
	jsval_t word_text;
	jsval_t limit_two;
	jsval_t search_result;
	jsval_t iterator;
	jsval_t iterator_result;
	jsval_t callback_result;
	jsval_t split_result;
	jsval_t value;
	generated_replace_callback_ctx_t ctx = {0, 0};
	jsmethod_error_t error;
	generated_status_t status;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_string_new_utf16(&region, digit_subject_units,
			sizeof(digit_subject_units) / sizeof(digit_subject_units[0]),
			&digit_text) < 0
			|| jsval_string_new_utf16(&region, whitespace_subject_units,
				sizeof(whitespace_subject_units) /
				sizeof(whitespace_subject_units[0]), &whitespace_text) < 0
			|| jsval_string_new_utf16(&region, word_subject_units,
				sizeof(word_subject_units) /
				sizeof(word_subject_units[0]), &word_text) < 0
			|| jsval_string_new_utf8(&region, (const uint8_t *)"2", 1,
				&limit_two) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_string_new_utf16/utf8(predefined class args)");
	}

	if (jsval_method_string_search_u_digit_class(&region, digit_text,
			&search_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_search_u_digit_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_strict_eq(&region, search_result, jsval_number(3.0)) != 1) {
		return generated_failf(detail, cap,
				"expected predefined digit search index 3");
	}

	if (jsval_method_string_match_all_u_word_class(&region, word_text,
			&iterator, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_match_all_u_word_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_match_iterator_next(&region, iterator, &done, &iterator_result,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_match_iterator_next(predefined word first) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (done || iterator_result.kind != JSVAL_KIND_OBJECT) {
		return generated_failf(detail, cap,
				"expected first predefined word matchAll result");
	}
	if (jsval_object_get_utf8(&region, iterator_result,
			(const uint8_t *)"0", 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_get_utf8(predefined word first capture)");
	}
	status = generated_expect_utf16_string(&region, value, a_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_replace_all_u_negated_word_class_fn(&region,
			word_text, generated_replace_offset_callback, &ctx,
			&callback_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_replace_all_u_negated_word_class_fn failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (ctx.call_count != 3) {
		return generated_failf(detail, cap,
				"expected 3 predefined negated-word callback calls, got %d",
				ctx.call_count);
	}
	status = generated_expect_utf16_string(&region, callback_result,
			callback_expected,
			sizeof(callback_expected) / sizeof(callback_expected[0]),
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_split_u_whitespace_class(&region, whitespace_text,
			1, limit_two, &split_result, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_split_u_whitespace_class failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (split_result.kind != JSVAL_KIND_ARRAY
			|| jsval_array_length(&region, split_result) != 2) {
		return generated_failf(detail, cap,
				"expected 2-entry predefined whitespace split result");
	}
	if (jsval_array_get(&region, split_result, 0, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(predefined whitespace split 0)");
	}
	status = generated_expect_utf16_string(&region, value, a_unit, 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	if (jsval_array_get(&region, split_result, 1, &value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_array_get(predefined whitespace split 1)");
	}
	return generated_expect_utf16_string(&region, value,
			(const uint16_t[]){'B'}, 1, detail, cap);
}
#endif

static generated_status_t generated_smoke_jsval_method_accessor(char *detail,
		size_t cap)
{
	static const uint8_t input[] =
		"{\"ascii\":\"abc\",\"astral\":\"\\ud83d\\ude00\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t ascii;
	jsval_t astral;
	jsval_t value;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"ascii", 5,
			&ascii) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(ascii)");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"astral", 6,
			&astral) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(astral)");
	}

	if (jsval_method_string_char_at(&region, ascii, 1, jsval_number(1.9),
			&value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_char_at failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"b", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_at(&region, ascii, 1, jsval_number(-1.0), &value,
			&error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_at failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_string(&region, value, (const uint8_t *)"c", 1,
			detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_char_code_at(&region, ascii, 1,
			jsval_number(INFINITY), &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_char_code_at failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_nan_value(value, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(0.0), &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_code_point_at failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	status = generated_expect_number(&region, value, 0x1F600, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(99.0), &value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_code_point_at(oob) failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (value.kind != JSVAL_KIND_UNDEFINED) {
		return generated_failf(detail, cap,
				"expected out-of-range codePointAt to return undefined");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsmethod_accessor_abrupt(
		char *detail, size_t cap)
{
	generated_callback_ctx_t throw_ctx = {1};
	jsmethod_error_t error;
	int has_value;
	double value;

	if (jsmethod_string_code_point_at(&has_value, &value,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			1, jsmethod_value_number(0.0), &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt accessor receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT accessor coercion, got %d",
				(int)error.kind);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_method_slice_substring(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"text\":\"bananas\"}";
	static const uint8_t expected_json[] =
		"{\"text\":\"bananas\",\"slice\":\"anana\",\"tail\":\"nas\",\"substring\":\"nan\",\"rest\":\"nanas\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t slice_value;
	jsval_t tail_value;
	jsval_t substring_value;
	jsval_t rest_value;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 16, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"text", 4,
			&text) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(text)");
	}

	if (jsval_method_string_slice(&region, text, 1, jsval_number(1.0), 1,
			jsval_number(-1.0), &slice_value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_slice failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_slice(&region, text, 1, jsval_number(-3.0), 0,
			jsval_undefined(), &tail_value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_slice tail failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_substring(&region, text, 1, jsval_number(5.0), 1,
			jsval_number(2.0), &substring_value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_substring failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_substring(&region, text, 1, jsval_number(2.0), 0,
			jsval_undefined(), &rest_value, &error) < 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_substring rest failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	status = generated_expect_string(&region, slice_value,
			(const uint8_t *)"anana", 5, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_string(&region, tail_value,
			(const uint8_t *)"nas", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_string(&region, substring_value,
			(const uint8_t *)"nan", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_string(&region, rest_value,
			(const uint8_t *)"nanas", 5, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 5) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_region_set_root(&region, root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_set_root");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"slice", 5,
			slice_value) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"tail", 4,
			tail_value) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"substring", 9,
			substring_value) < 0 ||
			jsval_object_set_utf8(&region, root, (const uint8_t *)"rest", 4,
			rest_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(slice/substring)");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_slice_substring_abrupt(
		char *detail, size_t cap)
{
	jsmethod_error_t error;
	uint16_t storage[8];
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_substring(&out,
			jsmethod_value_coercible(NULL, generated_type_error_to_string),
			0, jsmethod_value_undefined(),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_failf(detail, cap,
				"expected substring receiver coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_TYPE) {
		return generated_failf(detail, cap,
				"expected TYPE receiver coercion, got %d",
				(int)error.kind);
	}

	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_method_trim_repeat(
		char *detail, size_t cap)
{
	static const uint8_t json[] = "{\"trim\":\"\\ufefffoo\\n\",\"repeat\":\"ha\"}";
	static const uint8_t expected_json[] =
			"{\"trim\":\"\\ufefffoo\\n\",\"repeat\":\"ha\",\"trimmed\":\"foo\",\"repeated\":\"hahaha\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t trim_text;
	jsval_t repeat_text;
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_repeat_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, json, sizeof(json) - 1, 16, &root) != 0) {
		return generated_failf(detail, cap, "failed to parse JSON input");
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 4) != 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_object_get_utf8(&region, root,
			(const uint8_t *)"trim", 4, &trim_text) != 0) {
		return generated_failf(detail, cap, "failed to fetch trim string");
	}
	if (jsval_object_get_utf8(&region, root,
			(const uint8_t *)"repeat", 6, &repeat_text) != 0) {
		return generated_failf(detail, cap, "failed to fetch repeat string");
	}
	if (jsval_method_string_trim(&region, trim_text, &result, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_trim failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_set_utf8(&region, root,
			(const uint8_t *)"trimmed", 7, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(trimmed)");
	}
	if (jsval_method_string_repeat_measure(&region, repeat_text, 1,
			jsval_number(3.0), &sizes, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_repeat_measure failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 6) {
		return generated_failf(detail, cap,
				"expected repeat result_len 6, got %zu", sizes.result_len);
	}
	if (jsval_method_string_repeat(&region, repeat_text, 1,
			jsval_number(3.0), &result, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_repeat failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_set_utf8(&region, root,
			(const uint8_t *)"repeated", 8, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(repeated)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsval_method_substr_trim_alias(
		char *detail, size_t cap)
{
	static const uint8_t json[] = "{\"text\":\"bananas\",\"trim\":\"\\ufefffoo\\n\"}";
	static const uint8_t expected_json[] =
			"{\"text\":\"bananas\",\"trim\":\"\\ufefffoo\\n\",\"substr\":\"ana\",\"tail\":\"nas\",\"trimLeft\":\"foo\\n\",\"trimRight\":\""
			"\xEF\xBB\xBF"
			"foo\"}";
	static const uint8_t trim_right_expected[] = {
		0xEF, 0xBB, 0xBF, 'f', 'o', 'o'
	};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t trim_text;
	jsval_t substr_value;
	jsval_t tail_value;
	jsval_t trim_left_value;
	jsval_t trim_right_value;
	jsmethod_error_t error;
	generated_status_t status;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, json, sizeof(json) - 1, 16, &root) != 0) {
		return generated_failf(detail, cap, "failed to parse JSON input");
	}
	if (jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) != 0) {
		return generated_failf(detail, cap, "failed to fetch text string");
	}
	if (jsval_object_get_utf8(&region, root,
			(const uint8_t *)"trim", 4, &trim_text) != 0) {
		return generated_failf(detail, cap, "failed to fetch trim string");
	}
	if (jsval_method_string_substr(&region, text, 1, jsval_number(1.0), 1,
			jsval_number(3.0), &substr_value, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_substr failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_substr(&region, text, 1, jsval_number(-3.0), 0,
			jsval_undefined(), &tail_value, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_substr tail failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_trim_left(&region, trim_text, &trim_left_value,
			&error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_trim_left failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_method_string_trim_right(&region, trim_text, &trim_right_value,
			&error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_trim_right failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}

	status = generated_expect_string(&region, substr_value,
			(const uint8_t *)"ana", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_string(&region, tail_value,
			(const uint8_t *)"nas", 3, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_string(&region, trim_left_value,
			(const uint8_t *)"foo\n", 4, detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}
	status = generated_expect_string(&region, trim_right_value,
			trim_right_expected, sizeof(trim_right_expected), detail, cap);
	if (status != GENERATED_PASS) {
		return status;
	}

	if (jsval_promote_object_shallow_in_place(&region, &root, 6) != 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_object_set_utf8(&region, root,
			(const uint8_t *)"substr", 6, substr_value) < 0 ||
			jsval_object_set_utf8(&region, root,
			(const uint8_t *)"tail", 4, tail_value) < 0 ||
			jsval_object_set_utf8(&region, root,
			(const uint8_t *)"trimLeft", 8, trim_left_value) < 0 ||
			jsval_object_set_utf8(&region, root,
			(const uint8_t *)"trimRight", 9, trim_right_value) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(substr/trim aliases)");
	}

	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_substr_abrupt(
		char *detail, size_t cap)
{
	generated_callback_ctx_t throw_ctx = {1};
	uint16_t storage[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_substr(&out,
			jsmethod_value_string_utf8((const uint8_t *)"bananas", 7), 1,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			0, jsmethod_value_undefined(), &error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt substr start coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT substr start coercion, got %d",
				(int)error.kind);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsmethod_repeat_abrupt(
		char *detail, size_t cap)
{
	generated_callback_ctx_t throw_ctx = {1};
	uint16_t storage[8];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_repeat(&out,
			jsmethod_value_string_utf8((const uint8_t *)"x", 1), 1,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt repeat count coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT repeat count, got %d", (int)error.kind);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_smoke_jsval_method_pad(
		char *detail, size_t cap)
{
	static const uint8_t json[] = "{\"text\":\"abc\"}";
	static const uint8_t expected_json[] =
			"{\"text\":\"abc\",\"left\":\"defdabc\",\"right\":\"abc  \"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t filler;
	jsval_t result;
	jsmethod_error_t error;
	jsmethod_string_pad_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, json, sizeof(json) - 1, 16, &root) != 0) {
		return generated_failf(detail, cap, "failed to parse JSON input");
	}
	if (jsval_promote_object_shallow_in_place(&region, &root, 3) != 0) {
		return generated_fail_errno(detail, cap,
				"jsval_promote_object_shallow_in_place");
	}
	if (jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) != 0) {
		return generated_failf(detail, cap, "failed to fetch text string");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"def", 3, &filler) != 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8(filler)");
	}
	if (jsval_method_string_pad_start_measure(&region, text, 1,
			jsval_number(7.0), 1, filler, &sizes, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_pad_start_measure failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (sizes.result_len != 7) {
		return generated_failf(detail, cap,
				"expected padStart result_len 7, got %zu", sizes.result_len);
	}
	if (jsval_method_string_pad_start(&region, text, 1,
			jsval_number(7.0), 1, filler, &result, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_pad_start failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_set_utf8(&region, root,
			(const uint8_t *)"left", 4, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(left)");
	}
	if (jsval_method_string_pad_end(&region, text, 1,
			jsval_number(5.0), 0, jsval_undefined(), &result, &error) != 0) {
		return generated_failf(detail, cap,
				"jsval_method_string_pad_end failed: errno=%d kind=%d",
				errno, (int)error.kind);
	}
	if (jsval_object_set_utf8(&region, root,
			(const uint8_t *)"right", 5, result) < 0) {
		return generated_fail_errno(detail, cap,
				"jsval_object_set_utf8(right)");
	}
	return generated_expect_json(&region, root, expected_json,
			sizeof(expected_json) - 1, detail, cap);
}

static generated_status_t generated_smoke_jsmethod_pad_abrupt(
		char *detail, size_t cap)
{
	generated_callback_ctx_t throw_ctx = {1};
	uint16_t storage[16];
	jsmethod_error_t error;
	jsstr16_t out;

	jsstr16_init_from_buf(&out, (const char *)storage, sizeof(storage));
	if (jsmethod_string_pad_start(&out,
			jsmethod_value_string_utf8((const uint8_t *)"abc", 3), 1,
			jsmethod_value_number(5.0), 1,
			jsmethod_value_coercible(&throw_ctx, generated_callback_to_string),
			&error) == 0) {
		return generated_failf(detail, cap,
				"expected abrupt padStart fill coercion to fail");
	}
	if (error.kind != JSMETHOD_ERROR_ABRUPT) {
		return generated_failf(detail, cap,
				"expected ABRUPT fill coercion, got %d", (int)error.kind);
	}
	return GENERATED_PASS;
}

static generated_status_t generated_string_normalize_nfc_combining_ring(char *detail, size_t cap)
{
	static const uint8_t input[] = {'A', 0xCC, 0x8A};
	static const uint8_t expected[] = {0xC3, 0x85};
	uint8_t storage[32];
	uint32_t workspace[64];
	jsstr8_t value;

	jsstr8_init_from_buf(&value, (const char *)storage, sizeof(storage));
	jsstr8_set_from_utf8(&value, input, sizeof(input));
	if (jsstr8_normalize_buf(&value, workspace, sizeof(workspace) / sizeof(workspace[0])) < 0) {
		return generated_fail_errno(detail, cap, "jsstr8_normalize_buf");
	}

	return generated_expect_jsstr8(&value, expected, sizeof(expected), detail, cap);
}

static generated_status_t generated_string_utf16_length_surrogate_pair(char *detail, size_t cap)
{
	static const uint8_t input[] = {'A', 0xF0, 0x9F, 0x98, 0x80};
	uint8_t storage[32];
	jsstr8_t value;

	jsstr8_init_from_buf(&value, (const char *)storage, sizeof(storage));
	jsstr8_set_from_utf8(&value, input, sizeof(input));

	if (jsstr8_get_utf32len(&value) != 2) {
		return generated_failf(detail, cap, "expected 2 code points");
	}
	if (jsstr8_get_utf16len(&value) != 3) {
		return generated_failf(detail, cap, "expected 3 UTF-16 code units");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_string_concat_multibyte(char *detail, size_t cap)
{
	static const uint8_t left_input[] = "Jazz ";
	static const uint8_t right_input[] = {0xF0, 0x9F, 0x8E, 0xB7};
	static const uint8_t expected[] = {'J', 'a', 'z', 'z', ' ', 0xF0, 0x9F, 0x8E, 0xB7};
	uint8_t left_storage[64];
	uint8_t right_storage[16];
	jsstr8_t left;
	jsstr8_t right;

	jsstr8_init_from_buf(&left, (const char *)left_storage, sizeof(left_storage));
	jsstr8_init_from_buf(&right, (const char *)right_storage, sizeof(right_storage));
	jsstr8_set_from_utf8(&left, left_input, sizeof(left_input) - 1);
	jsstr8_set_from_utf8(&right, right_input, sizeof(right_input));
	if (jsstr8_concat(&left, &right) < 0) {
		return generated_fail_errno(detail, cap, "jsstr8_concat");
	}

	return generated_expect_jsstr8(&left, expected, sizeof(expected), detail, cap);
}

static generated_status_t generated_string_includes_multibyte(char *detail, size_t cap)
{
	static const uint8_t haystack_input[] = {'A', 0xF0, 0x9F, 0x98, 0x80, 'B'};
	static const uint8_t needle_input[] = {0xF0, 0x9F, 0x98, 0x80};
	uint8_t haystack_storage[32];
	uint8_t needle_storage[16];
	jsstr8_t haystack;
	jsstr8_t needle;

	jsstr8_init_from_buf(&haystack, (const char *)haystack_storage, sizeof(haystack_storage));
	jsstr8_init_from_buf(&needle, (const char *)needle_storage, sizeof(needle_storage));
	jsstr8_set_from_utf8(&haystack, haystack_input, sizeof(haystack_input));
	jsstr8_set_from_utf8(&needle, needle_input, sizeof(needle_input));
	if (!jsstr8_u8_includes(&haystack, &needle)) {
		return generated_failf(detail, cap, "expected multibyte substring to be found");
	}

	return GENERATED_PASS;
}

static generated_status_t generated_string_toupper_locale_tr(char *detail, size_t cap)
{
	static const uint8_t input[] = "istanbul";
	static const uint8_t expected[] = {0xC4, 0xB0, 'S', 'T', 'A', 'N', 'B', 'U', 'L'};
	uint8_t storage[64];
	jsstr8_t value;

	jsstr8_init_from_buf(&value, (const char *)storage, sizeof(storage));
	jsstr8_set_from_utf8(&value, input, sizeof(input) - 1);
	jsstr8_toupper_locale(&value, "tr");

	return generated_expect_jsstr8(&value, expected, sizeof(expected), detail, cap);
}

static generated_status_t generated_string_to_well_formed_invalid_utf8(char *detail, size_t cap)
{
	static const uint8_t invalid_input[] = {0x80, 'A'};
	static const uint8_t expected[] = {0xEF, 0xBF, 0xBD, 'A'};
	uint8_t dest_storage[16];
	jsstr8_t source;
	jsstr8_t dest;
	size_t written;

	source.cap = sizeof(invalid_input);
	source.len = sizeof(invalid_input);
	source.bytes = (uint8_t *)invalid_input;
	dest.cap = sizeof(dest_storage);
	dest.len = 0;
	dest.bytes = dest_storage;

	written = jsstr8_to_well_formed(&source, &dest);
	if (written != sizeof(expected)) {
		return generated_failf(detail, cap, "expected %zu bytes from jsstr8_to_well_formed, got %zu", sizeof(expected), written);
	}

	return generated_expect_jsstr8(&dest, expected, sizeof(expected), detail, cap);
}

static const generated_case_t generated_cases[] = {
	{"smoke", "json_promote_emit", generated_smoke_json_promote_emit},
	{"smoke", "jsval_values", generated_smoke_jsval_values},
	{"smoke", "jsval_typeof", generated_smoke_jsval_typeof},
	{"smoke", "jsval_symbol", generated_smoke_jsval_symbol},
	{"smoke", "jsval_bigint", generated_smoke_jsval_bigint},
	{"smoke", "jsval_function", generated_smoke_jsval_function},
	{"smoke", "jsval_date", generated_smoke_jsval_date},
	{"smoke", "jsval_crypto", generated_smoke_jsval_crypto},
	{"smoke", "jsval_promise", generated_smoke_jsval_promise},
	{"smoke", "jsval_set", generated_smoke_jsval_set},
	{"smoke", "jsval_map", generated_smoke_jsval_map},
	{"smoke", "jsval_iterators", generated_smoke_jsval_iterators},
	{"smoke", "jsval_url_core", generated_smoke_jsval_url_core},
	{"smoke", "jsval_nullish_coalescing",
		generated_smoke_jsval_nullish_coalescing},
	{"smoke", "jsval_ternary", generated_smoke_jsval_ternary},
	{"smoke", "jsval_strict_equality", generated_smoke_jsval_strict_equality},
	{"smoke", "jsval_logical_not", generated_smoke_jsval_logical_not},
	{"smoke", "jsval_logical_and_or", generated_smoke_jsval_logical_and_or},
	{"smoke", "jsval_numeric_coercion", generated_smoke_jsval_numeric_coercion},
	{"smoke", "jsval_bitwise", generated_smoke_jsval_bitwise},
	{"smoke", "jsval_shift", generated_smoke_jsval_shift},
	{"smoke", "jsval_relational", generated_smoke_jsval_relational},
	{"smoke", "jsval_abstract_equality",
		generated_smoke_jsval_abstract_equality},
	{"smoke", "jsval_json_value_parity", generated_smoke_jsval_json_value_parity},
	{"smoke", "jsval_shallow_planned_promotion", generated_smoke_jsval_shallow_planned_promotion},
	{"smoke", "jsval_native_containers", generated_smoke_jsval_native_containers},
	{"smoke", "jsval_lookup_capacity_contracts", generated_smoke_jsval_lookup_capacity_contracts},
	{"smoke", "jsval_object_key_order", generated_smoke_jsval_object_key_order},
	{"smoke", "jsval_object_value_order", generated_smoke_jsval_object_value_order},
	{"smoke", "jsval_object_copy_own", generated_smoke_jsval_object_copy_own},
	{"smoke", "jsval_object_clone_own", generated_smoke_jsval_object_clone_own},
	{"smoke", "jsval_object_spread_parity", generated_smoke_jsval_object_spread_parity},
	{"smoke", "jsval_array_clone_dense", generated_smoke_jsval_array_clone_dense},
	{"smoke", "jsval_array_spread_parity", generated_smoke_jsval_array_spread_parity},
	{"smoke", "jsval_array_splice_dense", generated_smoke_jsval_array_splice_dense},
	{"smoke", "jsval_dense_array_semantics", generated_smoke_jsval_dense_array_semantics},
	{"smoke", "jsval_method_locale_upper", generated_smoke_jsval_method_locale_upper},
	{"smoke", "jsval_method_normalize", generated_smoke_jsval_method_normalize},
	{"smoke", "jsval_method_lower", generated_smoke_jsval_method_lower},
	{"smoke", "jsval_method_is_well_formed", generated_smoke_jsval_method_is_well_formed},
	{"smoke", "jsval_method_concat", generated_smoke_jsval_method_concat},
	{"smoke", "jsval_method_replace", generated_smoke_jsval_method_replace},
	{"smoke", "jsval_method_replace_all",
		generated_smoke_jsval_method_replace_all},
	{"smoke", "jsval_method_replace_callback",
		generated_smoke_jsval_method_replace_callback},
	{"smoke", "jsval_method_replace_callback_abrupt",
		generated_smoke_jsval_method_replace_callback_abrupt},
	{"smoke", "jsval_method_accessor", generated_smoke_jsval_method_accessor},
	{"smoke", "jsval_method_slice_substring", generated_smoke_jsval_method_slice_substring},
	{"smoke", "jsval_method_trim_repeat", generated_smoke_jsval_method_trim_repeat},
	{"smoke", "jsval_method_substr_trim_alias", generated_smoke_jsval_method_substr_trim_alias},
	{"smoke", "jsval_method_pad", generated_smoke_jsval_method_pad},
	{"smoke", "jsval_method_search", generated_smoke_jsval_method_search},
	{"smoke", "jsval_method_split", generated_smoke_jsval_method_split},
#if JSMX_WITH_REGEX
	{"smoke", "jsval_regexp_core", generated_smoke_jsval_regexp_core},
	{"smoke", "jsval_regexp_exec_match", generated_smoke_jsval_regexp_exec_match},
	{"smoke", "jsval_regexp_exec_state", generated_smoke_jsval_regexp_exec_state},
	{"smoke", "jsval_regexp_match_all", generated_smoke_jsval_regexp_match_all},
	{"smoke", "jsval_regexp_named_groups", generated_smoke_jsval_regexp_named_groups},
	{"smoke", "jsval_regexp_named_groups_jit",
		generated_smoke_jsval_regexp_named_groups_jit},
	{"smoke", "jsval_regexp_constructor_choice",
		generated_smoke_jsval_regexp_constructor_choice},
	{"smoke", "jsval_u_literal_surrogate_rewrite",
		generated_smoke_jsval_u_literal_surrogate_rewrite},
	{"smoke", "jsval_u_literal_sequence_rewrite",
		generated_smoke_jsval_u_literal_sequence_rewrite},
	{"smoke", "jsval_u_literal_class_rewrite",
		generated_smoke_jsval_u_literal_class_rewrite},
	{"smoke", "jsval_u_literal_negated_class_rewrite",
		generated_smoke_jsval_u_literal_negated_class_rewrite},
	{"smoke", "jsval_u_literal_range_class_rewrite",
		generated_smoke_jsval_u_literal_range_class_rewrite},
	{"smoke", "jsval_u_literal_negated_range_class_rewrite",
		generated_smoke_jsval_u_literal_negated_range_class_rewrite},
	{"smoke", "jsval_u_predefined_class_rewrite",
		generated_smoke_jsval_u_predefined_class_rewrite},
	{"smoke", "jsval_method_regex_replace", generated_smoke_jsval_method_regex_replace},
	{"smoke", "jsval_method_regex_replace_all",
		generated_smoke_jsval_method_regex_replace_all},
	{"smoke", "jsval_method_regex_replace_callback",
		generated_smoke_jsval_method_regex_replace_callback},
	{"smoke", "jsval_method_regex_replace_named_groups",
		generated_smoke_jsval_method_regex_replace_named_groups},
	{"smoke", "jsval_method_regex_search", generated_smoke_jsval_method_regex_search},
	{"smoke", "jsval_method_regex_split", generated_smoke_jsval_method_regex_split},
#endif
	{"smoke", "jsmethod_accessor_abrupt", generated_smoke_jsmethod_accessor_abrupt},
	{"smoke", "jsmethod_concat_abrupt", generated_smoke_jsmethod_concat_abrupt},
	{"smoke", "jsmethod_replace_abrupt", generated_smoke_jsmethod_replace_abrupt},
	{"smoke", "jsmethod_replace_all_abrupt",
		generated_smoke_jsmethod_replace_all_abrupt},
	{"smoke", "jsmethod_slice_substring_abrupt", generated_smoke_jsmethod_slice_substring_abrupt},
	{"smoke", "jsmethod_substr_abrupt", generated_smoke_jsmethod_substr_abrupt},
	{"smoke", "jsmethod_repeat_abrupt", generated_smoke_jsmethod_repeat_abrupt},
	{"smoke", "jsmethod_pad_abrupt", generated_smoke_jsmethod_pad_abrupt},
	{"smoke", "jsmethod_search_abrupt", generated_smoke_jsmethod_search_abrupt},
	{"smoke", "jsmethod_split_abrupt", generated_smoke_jsmethod_split_abrupt},
#if JSMX_WITH_REGEX
	{"smoke", "jsmethod_regex_search_abrupt", generated_smoke_jsmethod_regex_search_abrupt},
#endif
	{"strings", "normalize_nfc_combining_ring", generated_string_normalize_nfc_combining_ring},
	{"strings", "utf16_length_surrogate_pair", generated_string_utf16_length_surrogate_pair},
	{"strings", "concat_multibyte", generated_string_concat_multibyte},
	{"strings", "includes_multibyte", generated_string_includes_multibyte},
	{"strings", "toupper_locale_tr", generated_string_toupper_locale_tr},
	{"strings", "to_well_formed_invalid_utf8", generated_string_to_well_formed_invalid_utf8},
};

int main(void)
{
	size_t i;
	size_t passed = 0;
	size_t unsupported = 0;
	size_t failed = 0;

	for (i = 0; i < sizeof(generated_cases) / sizeof(generated_cases[0]); i++) {
		char detail[256] = {0};
		generated_status_t status = generated_cases[i].run(detail, sizeof(detail));

		switch (status) {
		case GENERATED_PASS:
			passed++;
			printf("PASS %s/%s\n", generated_cases[i].suite, generated_cases[i].name);
			break;
		case GENERATED_KNOWN_UNSUPPORTED:
			unsupported++;
			printf("UNSUPPORTED %s/%s: %s\n", generated_cases[i].suite, generated_cases[i].name, detail[0] ? detail : "known unsupported");
			break;
		default:
			failed++;
			printf("FAIL %s/%s: %s\n", generated_cases[i].suite, generated_cases[i].name, detail[0] ? detail : "wrong result");
			break;
		}
	}

	printf("generated cases: %zu passed, %zu known unsupported, %zu wrong result\n", passed, unsupported, failed);
	return failed == 0 ? 0 : 1;
}

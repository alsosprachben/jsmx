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

static generated_status_t generated_smoke_jsval_dense_array_semantics(
		char *detail, size_t cap)
{
	static const uint8_t input[] = "[4,5]";
	static const uint8_t expected_json[] =
		"{\"native\":[1,9],\"parsed\":[4,5,6]}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t native_items;
	jsval_t parsed_items;
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
	{"smoke", "jsval_dense_array_semantics", generated_smoke_jsval_dense_array_semantics},
	{"smoke", "jsval_method_locale_upper", generated_smoke_jsval_method_locale_upper},
	{"smoke", "jsval_method_normalize", generated_smoke_jsval_method_normalize},
	{"smoke", "jsval_method_lower", generated_smoke_jsval_method_lower},
	{"smoke", "jsval_method_is_well_formed", generated_smoke_jsval_method_is_well_formed},
	{"smoke", "jsval_method_concat", generated_smoke_jsval_method_concat},
	{"smoke", "jsval_method_replace", generated_smoke_jsval_method_replace},
	{"smoke", "jsval_method_replace_all",
		generated_smoke_jsval_method_replace_all},
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
	{"smoke", "jsval_method_regex_replace", generated_smoke_jsval_method_regex_replace},
	{"smoke", "jsval_method_regex_replace_all",
		generated_smoke_jsval_method_regex_replace_all},
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

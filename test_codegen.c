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
	if (jsval_region_promote_root(&region, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_promote_root");
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
	{"smoke", "jsval_json_value_parity", generated_smoke_jsval_json_value_parity},
	{"smoke", "jsval_shallow_planned_promotion", generated_smoke_jsval_shallow_planned_promotion},
	{"smoke", "jsval_native_containers", generated_smoke_jsval_native_containers},
	{"smoke", "jsval_lookup_capacity_contracts", generated_smoke_jsval_lookup_capacity_contracts},
	{"smoke", "jsval_method_locale_upper", generated_smoke_jsval_method_locale_upper},
	{"smoke", "jsval_method_normalize", generated_smoke_jsval_method_normalize},
	{"smoke", "jsval_method_lower", generated_smoke_jsval_method_lower},
	{"smoke", "jsval_method_is_well_formed", generated_smoke_jsval_method_is_well_formed},
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

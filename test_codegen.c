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

static generated_status_t generated_smoke_json_promote_emit(char *detail, size_t cap)
{
	static const uint8_t input[] = "{\"message\":\"hi\",\"items\":[1,true,null]}";
	static const uint8_t expected[] = "{\"message\":\"line\\n\\\"quoted\\\"\",\"items\":[7,true,null]}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t items;
	jsval_t replacement;

	jsval_region_init(&region, storage, sizeof(storage));
	if (jsval_json_parse(&region, input, sizeof(input) - 1, 32, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_json_parse");
	}
	if (!jsval_is_json_backed(root)) {
		return generated_failf(detail, cap, "expected parsed root to stay JSON-backed");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(items)");
	}

	errno = 0;
	if (jsval_array_set(&region, items, 0, jsval_number(7.0)) == 0) {
		return generated_failf(detail, cap, "JSON-backed array mutation unexpectedly succeeded");
	}
	if (errno != ENOTSUP) {
		return generated_failf(detail, cap, "expected ENOTSUP before promotion, got %d", errno);
	}

	if (jsval_region_promote_root(&region, &root) < 0) {
		return generated_fail_errno(detail, cap, "jsval_region_promote_root");
	}
	if (!jsval_is_native(root)) {
		return generated_failf(detail, cap, "expected promoted root to be native");
	}
	if (jsval_object_get_utf8(&region, root, (const uint8_t *)"items", 5, &items) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_get_utf8(promoted items)");
	}
	if (!jsval_is_native(items)) {
		return generated_failf(detail, cap, "expected promoted child array to be native");
	}
	if (jsval_string_new_utf8(&region, (const uint8_t *)"line\n\"quoted\"", 13, &replacement) < 0) {
		return generated_fail_errno(detail, cap, "jsval_string_new_utf8");
	}
	if (jsval_object_set_utf8(&region, root, (const uint8_t *)"message", 7, replacement) < 0) {
		return generated_fail_errno(detail, cap, "jsval_object_set_utf8");
	}
	if (jsval_array_set(&region, items, 0, jsval_number(7.0)) < 0) {
		return generated_fail_errno(detail, cap, "jsval_array_set");
	}

	return generated_expect_json(&region, root, expected, sizeof(expected) - 1, detail, cap);
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

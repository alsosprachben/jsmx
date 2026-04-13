#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-headers-parity"

static int
make_string(jsval_region_t *region, const char *text, jsval_t *value_ptr)
{
	return jsval_string_new_utf8(region, (const uint8_t *)text, strlen(text),
			value_ptr);
}

static int
expect_string(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	size_t len = 0;
	size_t expected_len = strlen(expected);
	uint8_t buf[expected_len ? expected_len : 1];

	if (value.kind != JSVAL_KIND_STRING) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected string kind", label);
	}
	if (jsval_string_copy_utf8(region, value, NULL, 0, &len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure", label);
	}
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu", label, expected_len, len);
	}
	if (jsval_string_copy_utf8(region, value, buf, len, NULL) < 0
			|| memcmp(buf, expected, len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected bytes", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t headers;
	jsval_t name;
	jsval_t value;
	jsval_t got;
	int has = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_headers_new(&region,
			JSVAL_HEADERS_GUARD_NONE, &headers) == 0, SUITE, CASE_NAME,
			"failed to construct Headers");

	GENERATED_TEST_ASSERT(make_string(&region, "Content-Type", &name) == 0
			&& make_string(&region, "application/json", &value) == 0
			&& jsval_headers_append(&region, headers, name, value) == 0,
			SUITE, CASE_NAME, "append Content-Type");

	GENERATED_TEST_ASSERT(make_string(&region, "X-Custom", &name) == 0
			&& make_string(&region, "  first  ", &value) == 0
			&& jsval_headers_append(&region, headers, name, value) == 0,
			SUITE, CASE_NAME, "append X-Custom 1");
	GENERATED_TEST_ASSERT(make_string(&region, "X-Custom", &name) == 0
			&& make_string(&region, "second", &value) == 0
			&& jsval_headers_append(&region, headers, name, value) == 0,
			SUITE, CASE_NAME, "append X-Custom 2");

	GENERATED_TEST_ASSERT(make_string(&region, "content-type", &name) == 0
			&& jsval_headers_has(&region, headers, name, &has) == 0
			&& has == 1, SUITE, CASE_NAME, "has content-type");

	GENERATED_TEST_ASSERT(make_string(&region, "CONTENT-TYPE", &name) == 0
			&& jsval_headers_get(&region, headers, name, &got) == 0,
			SUITE, CASE_NAME, "get CONTENT-TYPE");
	GENERATED_TEST_ASSERT(expect_string(&region, got, "application/json",
			"content-type") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"content-type value mismatch");

	GENERATED_TEST_ASSERT(make_string(&region, "x-custom", &name) == 0
			&& jsval_headers_get(&region, headers, name, &got) == 0,
			SUITE, CASE_NAME, "get x-custom");
	GENERATED_TEST_ASSERT(expect_string(&region, got, "first, second",
			"x-custom") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"x-custom combined value mismatch");

	GENERATED_TEST_ASSERT(make_string(&region, "missing", &name) == 0
			&& jsval_headers_get(&region, headers, name, &got) == 0
			&& got.kind == JSVAL_KIND_NULL, SUITE, CASE_NAME,
			"missing header should be null");

	return generated_test_pass(SUITE, CASE_NAME);
}

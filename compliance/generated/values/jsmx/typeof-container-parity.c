#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/typeof-container-parity"

static int
expect_string(jsval_region_t *region, jsval_t value,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	size_t actual_len = 0;
	uint8_t buf[expected_len ? expected_len : 1];

	if (jsval_string_copy_utf8(region, value, NULL, 0, &actual_len) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to measure string result", label);
	}
	if (actual_len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: expected %zu bytes, got %zu",
				label, expected_len, actual_len);
	}
	if (jsval_string_copy_utf8(region, value, buf, actual_len, NULL) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to copy string result", label);
	}
	if (memcmp(buf, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: string result did not match expected output", label);
	}
	return GENERATED_TEST_PASS;
}

static int
expect_typeof(jsval_region_t *region, jsval_t value, const char *expected,
		const char *label)
{
	jsval_t result;

	if (jsval_typeof(region, value, &result) < 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: jsval_typeof failed", label);
	}
	return expect_string(region, result, (const uint8_t *)expected,
			strlen(expected), label);
}

int
main(void)
{
	static const uint8_t input[] = "{\"obj\":{},\"arr\":[]}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t obj;
	jsval_t arr;
	jsval_t native_obj;
	jsval_t native_arr;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 12,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"obj", 3, &obj) == 0,
			SUITE, CASE_NAME, "failed to read parsed object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"arr", 3, &arr) == 0,
			SUITE, CASE_NAME, "failed to read parsed array");
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &native_obj) == 0,
			SUITE, CASE_NAME, "failed to allocate native object");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 0, &native_arr) == 0,
			SUITE, CASE_NAME, "failed to allocate native array");

	GENERATED_TEST_ASSERT(expect_typeof(&region, root, "object",
			"typeof parsed root object") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed root result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, native_obj, "object",
			"typeof native object") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof native object result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, obj, "object",
			"typeof parsed object") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed object result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, native_arr, "object",
			"typeof native array") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof native array result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, arr, "object",
			"typeof parsed array") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed array result");

	return generated_test_pass(SUITE, CASE_NAME);
}

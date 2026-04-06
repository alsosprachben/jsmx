#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/typeof-primitive-parity"

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
	static const uint8_t input[] =
		"{\"flag\":true,\"num\":1,\"text\":\"x\",\"nothing\":null}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t flag;
	jsval_t num;
	jsval_t text;
	jsval_t nothing;
	jsval_t native_text;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 20,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON into a jsval region");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"flag", 4, &flag) == 0,
			SUITE, CASE_NAME, "failed to read flag");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"num", 3, &num) == 0,
			SUITE, CASE_NAME, "failed to read num");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) == 0,
			SUITE, CASE_NAME, "failed to read text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"nothing", 7, &nothing) == 0,
			SUITE, CASE_NAME, "failed to read nothing");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&native_text) == 0, SUITE, CASE_NAME,
			"failed to construct native string literal");

	GENERATED_TEST_ASSERT(expect_typeof(&region, jsval_undefined(), "undefined",
			"typeof undefined") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof undefined result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, jsval_null(), "object",
			"typeof null") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof null result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, jsval_bool(1), "boolean",
			"typeof true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof true result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, flag, "boolean",
			"typeof parsed true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed boolean result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, jsval_number(1.0), "number",
			"typeof 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof 1 result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, num, "number",
			"typeof parsed number") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed number result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, native_text, "string",
			"typeof native string") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof native string result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, text, "string",
			"typeof parsed string") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed string result");
	GENERATED_TEST_ASSERT(expect_typeof(&region, nothing, "object",
			"typeof parsed null") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected typeof parsed null result");

	return generated_test_pass(SUITE, CASE_NAME);
}

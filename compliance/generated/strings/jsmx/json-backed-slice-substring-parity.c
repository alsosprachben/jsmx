#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-slice-substring-parity"

static int
expect_string(jsval_region_t *region, jsval_t actual, const uint16_t *expected,
		size_t expected_len, const char *label)
{
	jsval_t expected_value;

	if (jsval_string_new_utf16(region, expected, expected_len, &expected_value) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: failed to build expected string", label);
	}
	if (!jsval_strict_eq(region, actual, expected_value)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected string result", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"bananas\"}";
	static const uint16_t anana[] = {0x0061, 0x006E, 0x0061, 0x006E, 0x0061};
	static const uint16_t nas[] = {0x006E, 0x0061, 0x0073};
	static const uint16_t nan[] = {0x006E, 0x0061, 0x006E};
	static const uint16_t nanas[] = {0x006E, 0x0061, 0x006E, 0x0061, 0x0073};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0, SUITE, CASE_NAME,
			"failed to fetch parsed text");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"bananas", 7, &native_text) == 0,
			SUITE, CASE_NAME, "failed to create native text");

	GENERATED_TEST_ASSERT(jsval_method_string_slice(&region, parsed_text, 1,
			jsval_number(1.0), 1, jsval_number(-1.0), &result, &error) == 0,
			SUITE, CASE_NAME, "failed parsed slice(1, -1)");
	GENERATED_TEST_ASSERT(expect_string(&region, result, anana, 5,
			"parsed slice(1, -1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed slice(1, -1)");

	GENERATED_TEST_ASSERT(jsval_method_string_slice(&region, native_text, 1,
			jsval_number(-3.0), 0, jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "failed native slice(-3)");
	GENERATED_TEST_ASSERT(expect_string(&region, result, nas, 3,
			"native slice(-3)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected native slice(-3)");

	GENERATED_TEST_ASSERT(jsval_method_string_substring(&region, parsed_text, 1,
			jsval_number(5.0), 1, jsval_number(2.0), &result, &error) == 0,
			SUITE, CASE_NAME, "failed parsed substring(5, 2)");
	GENERATED_TEST_ASSERT(expect_string(&region, result, nan, 3,
			"parsed substring(5, 2)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed substring(5, 2)");

	GENERATED_TEST_ASSERT(jsval_method_string_substring(&region, native_text, 1,
			jsval_number(2.0), 0, jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "failed native substring(2)");
	GENERATED_TEST_ASSERT(expect_string(&region, result, nanas, 5,
			"native substring(2)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected native substring(2)");
	return generated_test_pass(SUITE, CASE_NAME);
}

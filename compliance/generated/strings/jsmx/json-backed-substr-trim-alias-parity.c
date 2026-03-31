#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-substr-trim-alias-parity"

static int
expect_same(jsval_region_t *region, jsval_t actual, jsval_t expected,
		const char *label)
{
	int result;

	if (jsval_strict_eq(region, actual, expected)) {
		return GENERATED_TEST_PASS;
	}
	if (jsval_abstract_eq(region, actual, expected, &result) == 0 && result) {
		return GENERATED_TEST_PASS;
	}
	return generated_test_fail(SUITE, CASE_NAME,
			"%s: unexpected result mismatch", label);
}

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"bananas\",\"trim\":\"\\ufefffoo\\n\"}";
	static const uint8_t native_trim_bytes[] = {
		0xEF, 0xBB, 0xBF, 'f', 'o', 'o', '\n'
	};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t parsed_trim;
	jsval_t native_text;
	jsval_t native_trim;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"trim", 4, &parsed_trim) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed trim string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"bananas", 7, &native_text) == 0,
			SUITE, CASE_NAME, "failed to create native text");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			native_trim_bytes, sizeof(native_trim_bytes), &native_trim) == 0,
			SUITE, CASE_NAME, "failed to create native trim string");

	GENERATED_TEST_ASSERT(jsval_method_string_substr(&region, parsed_text, 1,
			jsval_number(1.0), 1, jsval_number(3.0), &parsed_result,
			&error) == 0, SUITE, CASE_NAME, "failed parsed substr");
	GENERATED_TEST_ASSERT(jsval_method_string_substr(&region, native_text, 1,
			jsval_number(1.0), 1, jsval_number(3.0), &native_result,
			&error) == 0, SUITE, CASE_NAME, "failed native substr");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"substr") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected substr parity");

	GENERATED_TEST_ASSERT(jsval_method_string_trim_left(&region, parsed_trim,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed trimLeft");
	GENERATED_TEST_ASSERT(jsval_method_string_trim_left(&region, native_trim,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native trimLeft");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"trimLeft") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimLeft parity");

	GENERATED_TEST_ASSERT(jsval_method_string_trim_right(&region, parsed_trim,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed trimRight");
	GENERATED_TEST_ASSERT(jsval_method_string_trim_right(&region, native_trim,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native trimRight");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"trimRight") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimRight parity");
	return generated_test_pass(SUITE, CASE_NAME);
}

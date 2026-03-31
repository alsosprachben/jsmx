#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-pad-parity"

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
	static const uint8_t json[] = "{\"text\":\"abc\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t filler;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;
	jsmethod_string_pad_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 8,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed text string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"abc", 3, &native_text) == 0,
			SUITE, CASE_NAME, "failed to create native text string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"def", 3, &filler) == 0,
			SUITE, CASE_NAME, "failed to create filler string");

	GENERATED_TEST_ASSERT(jsval_method_string_pad_start_measure(&region,
			parsed_text, 1, jsval_number(7.0), 1, filler, &sizes,
			&error) == 0, SUITE, CASE_NAME,
			"failed parsed padStart measure");
	GENERATED_TEST_ASSERT(sizes.result_len == 7, SUITE, CASE_NAME,
			"expected padStart result_len 7, got %zu", sizes.result_len);
	GENERATED_TEST_ASSERT(jsval_method_string_pad_start(&region,
			parsed_text, 1, jsval_number(7.0), 1, filler,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed padStart");
	GENERATED_TEST_ASSERT(jsval_method_string_pad_start(&region,
			native_text, 1, jsval_number(7.0), 1, filler,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native padStart");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"padStart") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected padStart parity");

	GENERATED_TEST_ASSERT(jsval_method_string_pad_end_measure(&region,
			parsed_text, 1, jsval_number(7.0), 1, filler, &sizes,
			&error) == 0, SUITE, CASE_NAME,
			"failed parsed padEnd measure");
	GENERATED_TEST_ASSERT(sizes.result_len == 7, SUITE, CASE_NAME,
			"expected padEnd result_len 7, got %zu", sizes.result_len);
	GENERATED_TEST_ASSERT(jsval_method_string_pad_end(&region,
			parsed_text, 1, jsval_number(7.0), 1, filler,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed padEnd");
	GENERATED_TEST_ASSERT(jsval_method_string_pad_end(&region,
			native_text, 1, jsval_number(7.0), 1, filler,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native padEnd");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"padEnd") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected padEnd parity");

	GENERATED_TEST_ASSERT(jsval_method_string_pad_start(&region,
			parsed_text, 1, jsval_number(5.0), 0, jsval_undefined(),
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed default-space padStart");
	GENERATED_TEST_ASSERT(jsval_method_string_pad_start(&region,
			native_text, 1, jsval_number(5.0), 0, jsval_undefined(),
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native default-space padStart");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"padStart default space") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected default-space padStart parity");
	return generated_test_pass(SUITE, CASE_NAME);
}

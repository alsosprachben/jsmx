#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-trim-repeat-parity"

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
	static const uint8_t json[] = "{\"trim\":\"\\ufefffoo\\n\",\"repeat\":\"ha\"}";
	static const uint8_t native_trim_bytes[] = {
		0xEF, 0xBB, 0xBF, 'f', 'o', 'o', '\n'
	};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_trim;
	jsval_t parsed_repeat;
	jsval_t native_trim;
	jsval_t native_repeat;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;
	jsmethod_string_repeat_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"trim", 4, &parsed_trim) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed trim string");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"repeat", 6, &parsed_repeat) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed repeat string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			native_trim_bytes, sizeof(native_trim_bytes), &native_trim) == 0,
			SUITE, CASE_NAME, "failed to create native trim string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"ha", 2, &native_repeat) == 0,
			SUITE, CASE_NAME, "failed to create native repeat string");

	GENERATED_TEST_ASSERT(jsval_method_string_trim(&region, parsed_trim,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed trim");
	GENERATED_TEST_ASSERT(jsval_method_string_trim(&region, native_trim,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native trim");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"trim") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trim parity");

	GENERATED_TEST_ASSERT(jsval_method_string_trim_start(&region, parsed_trim,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed trimStart");
	GENERATED_TEST_ASSERT(jsval_method_string_trim_start(&region, native_trim,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native trimStart");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"trimStart") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimStart parity");

	GENERATED_TEST_ASSERT(jsval_method_string_trim_end(&region, parsed_trim,
			&parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed trimEnd");
	GENERATED_TEST_ASSERT(jsval_method_string_trim_end(&region, native_trim,
			&native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native trimEnd");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"trimEnd") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected trimEnd parity");

	GENERATED_TEST_ASSERT(jsval_method_string_repeat_measure(&region,
			parsed_repeat, 1, jsval_number(3.0), &sizes, &error) == 0,
			SUITE, CASE_NAME, "failed parsed repeat measure");
	GENERATED_TEST_ASSERT(sizes.result_len == 6, SUITE, CASE_NAME,
			"expected repeat size 6, got %zu", sizes.result_len);
	GENERATED_TEST_ASSERT(jsval_method_string_repeat(&region, parsed_repeat, 1,
			jsval_number(3.0), &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "failed parsed repeat");
	GENERATED_TEST_ASSERT(jsval_method_string_repeat(&region, native_repeat, 1,
			jsval_number(3.0), &native_result, &error) == 0,
			SUITE, CASE_NAME, "failed native repeat");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"repeat") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected repeat parity");
	return generated_test_pass(SUITE, CASE_NAME);
}

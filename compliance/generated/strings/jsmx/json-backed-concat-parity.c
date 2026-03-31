#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-concat-parity"

static int
expect_same(jsval_region_t *region, jsval_t actual, jsval_t expected,
		const char *label)
{
	if (!jsval_strict_eq(region, actual, expected)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: unexpected string mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"lego\",\"suffix\":\"A\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t parsed_suffix;
	jsval_t native_text;
	jsval_t native_suffix;
	jsval_t args[4];
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;
	jsmethod_string_concat_sizes_t sizes;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"suffix", 6, &parsed_suffix) == 0,
			SUITE, CASE_NAME, "failed to fetch parsed suffix");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"lego", 4, &native_text) == 0,
			SUITE, CASE_NAME, "failed to create native text");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"A", 1, &native_suffix) == 0,
			SUITE, CASE_NAME, "failed to create native suffix");

	args[0] = parsed_suffix;
	args[1] = jsval_null();
	args[2] = jsval_bool(1);
	args[3] = jsval_number(42.0);
	GENERATED_TEST_ASSERT(jsval_method_string_concat_measure(&region,
			parsed_text, 4, args, &sizes, &error) == 0, SUITE, CASE_NAME,
			"failed parsed concat measure");
	GENERATED_TEST_ASSERT(sizes.result_len == 15, SUITE, CASE_NAME,
			"expected parsed concat result_len 15, got %zu",
			sizes.result_len);
	GENERATED_TEST_ASSERT(jsval_method_string_concat(&region, parsed_text, 4,
			args, &parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed concat");

	args[0] = native_suffix;
	args[1] = jsval_null();
	args[2] = jsval_bool(1);
	args[3] = jsval_number(42.0);
	GENERATED_TEST_ASSERT(jsval_method_string_concat_measure(&region,
			native_text, 4, args, &sizes, &error) == 0, SUITE, CASE_NAME,
			"failed native concat measure");
	GENERATED_TEST_ASSERT(sizes.result_len == 15, SUITE, CASE_NAME,
			"expected native concat result_len 15, got %zu",
			sizes.result_len);
	GENERATED_TEST_ASSERT(jsval_method_string_concat(&region, native_text, 4,
			args, &native_result, &error) == 0, SUITE, CASE_NAME,
			"failed native concat");

	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"parsed/native concat parity") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected parsed/native concat parity");

	args[0] = native_suffix;
	args[1] = jsval_undefined();
	GENERATED_TEST_ASSERT(jsval_method_string_concat(&region, parsed_text, 2,
			args, &parsed_result, &error) == 0, SUITE, CASE_NAME,
			"failed mixed concat");
	GENERATED_TEST_ASSERT(jsval_method_string_concat(&region, native_text, 2,
			args, &native_result, &error) == 0, SUITE, CASE_NAME,
			"failed mixed native concat");
	GENERATED_TEST_ASSERT(expect_same(&region, parsed_result, native_result,
			"mixed parsed/native concat parity") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected mixed concat parity");
	return generated_test_pass(SUITE, CASE_NAME);
}

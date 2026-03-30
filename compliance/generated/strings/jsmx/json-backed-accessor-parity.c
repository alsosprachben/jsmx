#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-accessor-parity"

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
	static const uint8_t json[] = "{\"ascii\":\"abc\",\"astral\":\"\\ud83d\\ude00\"}";
	static const uint16_t ascii_b[] = {0x0062};
	static const uint16_t ascii_c[] = {0x0063};
	static const uint16_t trail[] = {0xDE00};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t ascii;
	jsval_t astral;
	jsval_t native_ascii;
	jsval_t native_astral;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"ascii", 5, &ascii) == 0, SUITE, CASE_NAME,
			"failed to fetch parsed ascii");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"astral", 6, &astral) == 0, SUITE, CASE_NAME,
			"failed to fetch parsed astral");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"abc", 3, &native_ascii) == 0,
			SUITE, CASE_NAME, "failed to create native ascii");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region,
			(const uint16_t[]){0xD83D, 0xDE00}, 2, &native_astral) == 0,
			SUITE, CASE_NAME, "failed to create native astral");

	GENERATED_TEST_ASSERT(jsval_method_string_char_at(&region, ascii, 1,
			jsval_number(1.9), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed charAt");
	GENERATED_TEST_ASSERT(expect_string(&region, result, ascii_b, 1,
			"parsed charAt") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected parsed charAt result");

	GENERATED_TEST_ASSERT(jsval_method_string_at(&region, native_ascii, 1,
			jsval_number(-1.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed native at");
	GENERATED_TEST_ASSERT(expect_string(&region, result, ascii_c, 1,
			"native at") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected native at result");

	GENERATED_TEST_ASSERT(jsval_method_string_char_code_at(&region, ascii, 1,
			jsval_number(2.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed charCodeAt");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_number(99.0)) == 1,
			SUITE, CASE_NAME, "unexpected parsed charCodeAt result");

	GENERATED_TEST_ASSERT(jsval_method_string_char_code_at(&region, native_ascii, 1,
			jsval_number(INFINITY), &result, &error) == 0, SUITE, CASE_NAME,
			"failed native charCodeAt");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_NUMBER &&
			isnan(result.as.number), SUITE, CASE_NAME,
			"expected NaN from native charCodeAt(Infinity)");

	GENERATED_TEST_ASSERT(jsval_method_string_code_point_at(&region, astral, 1,
			jsval_number(0.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed codePointAt");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(0x1F600)) == 1, SUITE, CASE_NAME,
			"unexpected parsed codePointAt result");

	GENERATED_TEST_ASSERT(jsval_method_string_at(&region, native_astral, 1,
			jsval_number(-1.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed native astral at(-1)");
	GENERATED_TEST_ASSERT(expect_string(&region, result, trail, 1,
			"native astral at(-1)") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected native astral at(-1) result");

	GENERATED_TEST_ASSERT(jsval_method_string_code_point_at(&region, native_astral,
			1, jsval_number(99.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed native out-of-range codePointAt");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_UNDEFINED, SUITE, CASE_NAME,
			"expected undefined from out-of-range codePointAt");
	return generated_test_pass(SUITE, CASE_NAME);
}

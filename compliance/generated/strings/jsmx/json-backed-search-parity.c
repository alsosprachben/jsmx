#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-search-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"bananas\",\"needle\":\"na\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t text;
	jsval_t needle;
	jsval_t native_text;
	jsval_t native_needle;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "failed to parse JSON input");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) == 0, SUITE, CASE_NAME,
			"failed to fetch parsed text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"needle", 6, &needle) == 0, SUITE, CASE_NAME,
			"failed to fetch parsed needle");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"bananas", 7, &native_text) == 0,
			SUITE, CASE_NAME, "failed to create native text");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"na", 2, &native_needle) == 0,
			SUITE, CASE_NAME, "failed to create native needle");

	GENERATED_TEST_ASSERT(jsval_method_string_index_of(&region, text, needle, 0,
			jsval_undefined(), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed indexOf");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1,
			SUITE, CASE_NAME, "unexpected parsed indexOf result");
	GENERATED_TEST_ASSERT(jsval_method_string_index_of(&region, native_text,
			native_needle, 0, jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "failed native indexOf");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1,
			SUITE, CASE_NAME, "unexpected native indexOf result");

	GENERATED_TEST_ASSERT(jsval_method_string_last_index_of(&region, text, needle,
			1, jsval_number(3.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed lastIndexOf");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result, jsval_number(2.0)) == 1,
			SUITE, CASE_NAME, "unexpected parsed lastIndexOf result");

	GENERATED_TEST_ASSERT(jsval_method_string_includes(&region, text, needle, 1,
			jsval_number(3.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed includes");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1,
			SUITE, CASE_NAME, "unexpected parsed includes result");

	GENERATED_TEST_ASSERT(jsval_method_string_starts_with(&region, text, needle,
			1, jsval_number(2.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed startsWith");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1,
			SUITE, CASE_NAME, "unexpected parsed startsWith result");

	GENERATED_TEST_ASSERT(jsval_method_string_ends_with(&region, text, needle, 1,
			jsval_number(4.0), &result, &error) == 0, SUITE, CASE_NAME,
			"failed parsed endsWith");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_BOOL && result.as.boolean == 1,
			SUITE, CASE_NAME, "unexpected parsed endsWith result");

	return generated_test_pass(SUITE, CASE_NAME);
}

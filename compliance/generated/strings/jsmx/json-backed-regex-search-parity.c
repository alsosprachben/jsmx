#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE_NAME "strings"
#define CASE_NAME "jsmx/json-backed-regex-search-parity"

int
main(void)
{
	static const uint8_t json[] =
		"{\"text\":\"fooBar\",\"pattern\":\"bar\",\"flags\":\"i\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t parsed_pattern;
	jsval_t parsed_flags;
	jsval_t native_text;
	jsval_t native_pattern;
	jsval_t native_flags;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE_NAME, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE_NAME, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"pattern", 7, &parsed_pattern) == 0,
			SUITE_NAME, CASE_NAME, "parsed pattern lookup failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"flags", 5, &parsed_flags) == 0,
			SUITE_NAME, CASE_NAME, "parsed flags lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"fooBar", 6, &native_text) == 0,
			SUITE_NAME, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"bar", 3, &native_pattern) == 0,
			SUITE_NAME, CASE_NAME, "native pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"i", 1, &native_flags) == 0,
			SUITE_NAME, CASE_NAME, "native flags build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_search_regex(&region, parsed_text,
			parsed_pattern, 1, parsed_flags, &parsed_result, &error) == 0,
			SUITE_NAME, CASE_NAME, "parsed regex search failed");
	GENERATED_TEST_ASSERT(jsval_method_string_search_regex(&region, native_text,
			native_pattern, 1, native_flags, &native_result, &error) == 0,
			SUITE_NAME, CASE_NAME, "native regex search failed");
	GENERATED_TEST_ASSERT(parsed_result.kind == JSVAL_KIND_NUMBER &&
			native_result.kind == JSVAL_KIND_NUMBER,
			SUITE_NAME, CASE_NAME, "regex search should return numbers");
	GENERATED_TEST_ASSERT(parsed_result.as.number == 3.0 &&
			native_result.as.number == 3.0,
			SUITE_NAME, CASE_NAME, "expected parsed/native parity at index 3");

	return generated_test_pass(SUITE_NAME, CASE_NAME);
}

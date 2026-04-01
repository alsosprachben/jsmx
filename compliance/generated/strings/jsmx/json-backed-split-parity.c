#include "compliance/generated/string_split_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-split-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a,b,,c\"}";
	static const char *expected_string[] = {"a", "b", "", "c"};
	static const char *expected_regex[] = {"a", ",", "b", ",", "", ",", "c"};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t comma;
	jsval_t capture_pattern;
	jsval_t capture_regex;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a,b,,c", 6, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)",", 1, &comma) == 0,
			SUITE, CASE_NAME, "comma build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, parsed_text, 1,
			comma, 0, jsval_undefined(), &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed string split failed");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, native_text, 1,
			comma, 0, jsval_undefined(), &native_result, &error) == 0,
			SUITE, CASE_NAME, "native string split failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region,
			parsed_result, expected_string, GENERATED_LEN(expected_string),
			SUITE, CASE_NAME, "parsed string separator") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed string separator mismatch");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region,
			native_result, expected_string, GENERATED_LEN(expected_string),
			SUITE, CASE_NAME, "native string separator") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native string separator mismatch");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"(,)", 3, &capture_pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, capture_pattern, 0,
			jsval_undefined(), &capture_regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, parsed_text, 1,
			capture_regex, 0, jsval_undefined(), &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed regex split failed");
	GENERATED_TEST_ASSERT(jsval_method_string_split(&region, native_text, 1,
			capture_regex, 0, jsval_undefined(), &native_result, &error) == 0,
			SUITE, CASE_NAME, "native regex split failed");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region,
			parsed_result, expected_regex, GENERATED_LEN(expected_regex),
			SUITE, CASE_NAME, "parsed regex separator") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed regex separator mismatch");
	GENERATED_TEST_ASSERT(generated_expect_split_array_strings(&region,
			native_result, expected_regex, GENERATED_LEN(expected_regex),
			SUITE, CASE_NAME, "native regex separator") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native regex separator mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

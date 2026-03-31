#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-regexp-match-parity"

int
main(void)
{
	static const char *expected[] = {"3", "3", "3"};
	uint8_t storage[32768];
	static const uint8_t json[] = "{\"text\":\"343443444\"}";
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t match_pattern;
	jsval_t match_flags;
	jsval_t match_regex;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 8,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"343443444", 9, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"3", 1, &match_pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"g", 1, &match_flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, match_pattern, 1,
			match_flags, &match_regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, parsed_text, 1,
			match_regex, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed match failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match(&region, native_text, 1,
			match_regex, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native match failed");
	GENERATED_TEST_ASSERT(generated_expect_match_array_strings(&region,
			parsed_result, expected, 3, SUITE, CASE_NAME, "parsed result")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed array mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_array_strings(&region,
			native_result, expected, 3, SUITE, CASE_NAME, "native result")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native array mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

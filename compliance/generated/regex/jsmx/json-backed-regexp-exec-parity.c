#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "regex"
#define CASE_NAME "jsmx/json-backed-regexp-exec-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a1b2\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t pattern;
	jsval_t parsed_global_regex;
	jsval_t native_global_regex;
	jsval_t parsed_non_global_regex;
	jsval_t native_non_global_regex;
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
			(const uint8_t *)"a1b2", 4, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"([0-9])", 7, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8_jit(&region, "([0-9])",
			"g", &parsed_global_regex, &error) == 0,
			SUITE, CASE_NAME, "parsed global regex failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8_jit(&region, "([0-9])",
			"g", &native_global_regex, &error) == 0,
			SUITE, CASE_NAME, "native global regex failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &parsed_non_global_regex, &error) == 0,
			SUITE, CASE_NAME, "parsed non-global regex failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &native_non_global_regex, &error) == 0,
			SUITE, CASE_NAME, "native non-global regex failed");

	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, parsed_global_regex,
			parsed_text, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed global exec #1 failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, native_global_regex,
			native_text, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native global exec #1 failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "0", "1", SUITE, CASE_NAME, "parsed #1 match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #1 match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "1", "1", SUITE, CASE_NAME, "parsed #1 capture")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #1 capture mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region,
			parsed_result, "index", 1.0, SUITE, CASE_NAME, "parsed #1 index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #1 index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "input", "a1b2", SUITE, CASE_NAME, "parsed #1 input")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #1 input mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			native_result, "0", "1", SUITE, CASE_NAME, "native #1 match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #1 match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			native_result, "1", "1", SUITE, CASE_NAME, "native #1 capture")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #1 capture mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region,
			native_result, "index", 1.0, SUITE, CASE_NAME, "native #1 index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #1 index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			native_result, "input", "a1b2", SUITE, CASE_NAME, "native #1 input")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #1 input mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			parsed_global_regex, JSREGEX_FLAG_GLOBAL, 1, 2,
			SUITE, CASE_NAME, "parsed #1 state") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #1 state mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			native_global_regex, JSREGEX_FLAG_GLOBAL, 1, 2,
			SUITE, CASE_NAME, "native #1 state") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #1 state mismatch");

	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, parsed_global_regex,
			parsed_text, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed global exec #2 failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, native_global_regex,
			native_text, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native global exec #2 failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "0", "2", SUITE, CASE_NAME, "parsed #2 match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #2 match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "1", "2", SUITE, CASE_NAME, "parsed #2 capture")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #2 capture mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region,
			parsed_result, "index", 3.0, SUITE, CASE_NAME, "parsed #2 index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #2 index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			native_result, "0", "2", SUITE, CASE_NAME, "native #2 match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #2 match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			native_result, "1", "2", SUITE, CASE_NAME, "native #2 capture")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #2 capture mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region,
			native_result, "index", 3.0, SUITE, CASE_NAME, "native #2 index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #2 index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			parsed_global_regex, JSREGEX_FLAG_GLOBAL, 1, 4,
			SUITE, CASE_NAME, "parsed #2 state") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed #2 state mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			native_global_regex, JSREGEX_FLAG_GLOBAL, 1, 4,
			SUITE, CASE_NAME, "native #2 state") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native #2 state mismatch");

	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, parsed_global_regex,
			parsed_text, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed global exec #3 failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, native_global_regex,
			native_text, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native global exec #3 failed");
	GENERATED_TEST_ASSERT(parsed_result.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "parsed miss should be null");
	GENERATED_TEST_ASSERT(native_result.kind == JSVAL_KIND_NULL,
			SUITE, CASE_NAME, "native miss should be null");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			parsed_global_regex, JSREGEX_FLAG_GLOBAL, 1, 0,
			SUITE, CASE_NAME, "parsed miss state") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed miss state mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			native_global_regex, JSREGEX_FLAG_GLOBAL, 1, 0,
			SUITE, CASE_NAME, "native miss state") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native miss state mismatch");

	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region,
			parsed_non_global_regex, 7) == 0,
			SUITE, CASE_NAME, "parsed non-global lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region,
			native_non_global_regex, 7) == 0,
			SUITE, CASE_NAME, "native non-global lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, parsed_non_global_regex,
			parsed_text, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed non-global exec failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, native_non_global_regex,
			native_text, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native non-global exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "0", "1", SUITE, CASE_NAME, "parsed non-global match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed non-global match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region,
			parsed_result, "index", 1.0, SUITE, CASE_NAME, "parsed non-global index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed non-global index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			native_result, "0", "1", SUITE, CASE_NAME, "native non-global match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native non-global match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region,
			native_result, "index", 1.0, SUITE, CASE_NAME, "native non-global index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native non-global index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			parsed_non_global_regex, 0, 1, 7,
			SUITE, CASE_NAME, "parsed non-global state")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed non-global state mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region,
			native_non_global_regex, 0, 1, 7,
			SUITE, CASE_NAME, "native non-global state")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native non-global state mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

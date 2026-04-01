#include "compliance/generated/regex_core_helpers.h"

#define SUITE "regex"
#define CASE_NAME "jsmx/json-backed-regexp-core-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"abc\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t base_regex;
	jsval_t copy_regex;
	jsval_t override_regex;
	jsval_t y_flags;
	jsval_t flags_value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 8,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"abc", 3, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"y", 1, &y_flags) == 0,
			SUITE, CASE_NAME, "y flags build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "abc", "gim",
			&base_regex, &error) == 0, SUITE, CASE_NAME, "base regex failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_source(&region, base_regex,
			"abc", SUITE, CASE_NAME, "base source") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "base source mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, base_regex,
			jsval_regexp_global, 1, SUITE, CASE_NAME, "base global")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "base global mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, base_regex,
			jsval_regexp_ignore_case, 1, SUITE, CASE_NAME, "base ignoreCase")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "base ignoreCase mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, base_regex,
			jsval_regexp_multiline, 1, SUITE, CASE_NAME, "base multiline")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "base multiline mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, base_regex,
			jsval_regexp_dot_all, 0, SUITE, CASE_NAME, "base dotAll")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "base dotAll mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, base_regex,
			jsval_regexp_unicode, 0, SUITE, CASE_NAME, "base unicode")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "base unicode mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, base_regex,
			jsval_regexp_sticky, 0, SUITE, CASE_NAME, "base sticky")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "base sticky mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region, base_regex,
			JSREGEX_FLAG_GLOBAL | JSREGEX_FLAG_IGNORE_CASE
			| JSREGEX_FLAG_MULTILINE,
			0, 0, SUITE, CASE_NAME, "base info") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "base info mismatch");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, base_regex, 9) == 0,
			SUITE, CASE_NAME, "base lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, base_regex, 0,
			jsval_undefined(), &copy_regex, &error) == 0,
			SUITE, CASE_NAME, "copy regex failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_source(&region, copy_regex,
			"abc", SUITE, CASE_NAME, "copy source") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "copy source mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, copy_regex,
			"gim", SUITE, CASE_NAME, "copy flags") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "copy flags mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region, copy_regex,
			JSREGEX_FLAG_GLOBAL | JSREGEX_FLAG_IGNORE_CASE
			| JSREGEX_FLAG_MULTILINE,
			0, 0, SUITE, CASE_NAME, "copy info") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "copy info mismatch");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, base_regex, 1, y_flags,
			&override_regex, &error) == 0,
			SUITE, CASE_NAME, "override regex failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_source(&region, override_regex,
			"abc", SUITE, CASE_NAME, "override source") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "override source mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_flags(&region, override_regex,
			"y", SUITE, CASE_NAME, "override flags") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "override flags mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_boolean(&region, override_regex,
			jsval_regexp_sticky, 1, SUITE, CASE_NAME, "override sticky")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME, "override sticky mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region, override_regex,
			JSREGEX_FLAG_STICKY, 0, 0, SUITE, CASE_NAME, "override info")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "override info mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_test(&region, override_regex,
			parsed_text, 1, SUITE, CASE_NAME, "parsed text test")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed text test mismatch");
	GENERATED_TEST_ASSERT(generated_expect_regexp_info(&region, override_regex,
			JSREGEX_FLAG_STICKY, 0, 3, SUITE, CASE_NAME, "parsed state")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed state mismatch");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, override_regex, 0) == 0,
			SUITE, CASE_NAME, "override reset failed");
	GENERATED_TEST_ASSERT(generated_expect_regexp_test(&region, override_regex,
			native_text, 1, SUITE, CASE_NAME, "native text test")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native text test mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

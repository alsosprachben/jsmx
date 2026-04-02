#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-regex-replace-callback-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"1b2\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t parsed_regex;
	jsval_t native_regex;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;
	generated_replace_callback_record_t parsed_ctx = {
		.max_calls = 2,
		.replacement_text = "R"
	};
	generated_replace_callback_record_t native_ctx = {
		.max_calls = 2,
		.replacement_text = "R"
	};

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1b2", 3, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "([0-9])(b)?", "g",
			&parsed_regex, &error) == 0, SUITE, CASE_NAME,
			"parsed regex build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "([0-9])(b)?", "g",
			&native_regex, &error) == 0, SUITE, CASE_NAME,
			"native regex build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, parsed_text,
			parsed_regex, generated_record_replace_callback, &parsed_ctx,
			&parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed regex replace callback failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, native_text,
			native_regex, generated_record_replace_callback, &native_ctx,
			&native_result, &error) == 0,
			SUITE, CASE_NAME, "native regex replace callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "RR", SUITE, CASE_NAME, "parsed regex replace") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed regex replace mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "RR", SUITE, CASE_NAME, "native regex replace") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native regex replace mismatch");

	GENERATED_TEST_ASSERT(parsed_ctx.call_count == 2, SUITE, CASE_NAME,
			"expected parsed callback twice, got %zu", parsed_ctx.call_count);
	GENERATED_TEST_ASSERT(native_ctx.call_count == 2, SUITE, CASE_NAME,
			"expected native callback twice, got %zu", native_ctx.call_count);
	GENERATED_TEST_ASSERT(strcmp(parsed_ctx.input, "1b2") == 0, SUITE,
			CASE_NAME, "unexpected parsed callback input");
	GENERATED_TEST_ASSERT(strcmp(native_ctx.input, "1b2") == 0, SUITE,
			CASE_NAME, "unexpected native callback input");
	GENERATED_TEST_ASSERT(strcmp(parsed_ctx.matches[0], "1b") == 0 &&
			strcmp(parsed_ctx.matches[1], "2") == 0, SUITE, CASE_NAME,
			"unexpected parsed callback matches");
	GENERATED_TEST_ASSERT(strcmp(native_ctx.matches[0], "1b") == 0 &&
			strcmp(native_ctx.matches[1], "2") == 0, SUITE, CASE_NAME,
			"unexpected native callback matches");
	GENERATED_TEST_ASSERT(parsed_ctx.capture_defined[0][0] == 1 &&
			strcmp(parsed_ctx.captures[0][0], "1") == 0 &&
			parsed_ctx.capture_defined[0][1] == 1 &&
			strcmp(parsed_ctx.captures[0][1], "b") == 0, SUITE, CASE_NAME,
			"unexpected parsed first capture set");
	GENERATED_TEST_ASSERT(parsed_ctx.capture_defined[1][0] == 1 &&
			strcmp(parsed_ctx.captures[1][0], "2") == 0 &&
			parsed_ctx.capture_defined[1][1] == 0, SUITE, CASE_NAME,
			"unexpected parsed second capture set");
	GENERATED_TEST_ASSERT(native_ctx.capture_defined[0][0] == 1 &&
			strcmp(native_ctx.captures[0][0], "1") == 0 &&
			native_ctx.capture_defined[0][1] == 1 &&
			strcmp(native_ctx.captures[0][1], "b") == 0, SUITE, CASE_NAME,
			"unexpected native first capture set");
	GENERATED_TEST_ASSERT(native_ctx.capture_defined[1][0] == 1 &&
			strcmp(native_ctx.captures[1][0], "2") == 0 &&
			native_ctx.capture_defined[1][1] == 0, SUITE, CASE_NAME,
			"unexpected native second capture set");
	return generated_test_pass(SUITE, CASE_NAME);
}

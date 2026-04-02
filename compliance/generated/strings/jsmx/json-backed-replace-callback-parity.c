#include "compliance/generated/string_replace_callback_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-replace-callback-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a1b1\",\"search\":\"1\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t parsed_search;
	jsval_t native_text;
	jsval_t native_search;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;
	generated_replace_callback_record_t replace_ctx = {
		.max_calls = 1,
		.return_offset_number = 1,
		.offset_delta = 10.0
	};
	generated_replace_callback_record_t native_replace_ctx = {
		.max_calls = 1,
		.return_offset_number = 1,
		.offset_delta = 10.0
	};
	generated_replace_callback_record_t replace_all_ctx = {
		.max_calls = 2,
		.return_offset_number = 1,
		.offset_delta = 10.0
	};
	generated_replace_callback_record_t native_replace_all_ctx = {
		.max_calls = 2,
		.return_offset_number = 1,
		.offset_delta = 10.0
	};

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"search", 6, &parsed_search) == 0,
			SUITE, CASE_NAME, "parsed search lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a1b1", 4, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1", 1, &native_search) == 0,
			SUITE, CASE_NAME, "native search build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, parsed_text,
			parsed_search, generated_record_replace_callback, &replace_ctx,
			&parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed replace callback failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_fn(&region, native_text,
			native_search, generated_record_replace_callback,
			&native_replace_ctx, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native replace callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a11b1", SUITE, CASE_NAME, "parsed replace") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed replace mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "a11b1", SUITE, CASE_NAME, "native replace") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native replace mismatch");
	GENERATED_TEST_ASSERT(replace_ctx.call_count == 1, SUITE, CASE_NAME,
			"expected parsed replace callback once, got %zu",
			replace_ctx.call_count);
	GENERATED_TEST_ASSERT(native_replace_ctx.call_count == 1, SUITE, CASE_NAME,
			"expected native replace callback once, got %zu",
			native_replace_ctx.call_count);

	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region,
			parsed_text, parsed_search, generated_record_replace_callback,
			&replace_all_ctx, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed replaceAll callback failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all_fn(&region,
			native_text, native_search, generated_record_replace_callback,
			&native_replace_all_ctx, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native replaceAll callback failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a11b13", SUITE, CASE_NAME, "parsed replaceAll") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed replaceAll mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "a11b13", SUITE, CASE_NAME, "native replaceAll") ==
			GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native replaceAll mismatch");
	GENERATED_TEST_ASSERT(replace_all_ctx.call_count == 2, SUITE, CASE_NAME,
			"expected parsed replaceAll callback twice, got %zu",
			replace_all_ctx.call_count);
	GENERATED_TEST_ASSERT(native_replace_all_ctx.call_count == 2, SUITE,
			CASE_NAME, "expected native replaceAll callback twice, got %zu",
			native_replace_all_ctx.call_count);
	GENERATED_TEST_ASSERT(strcmp(replace_all_ctx.input, "a1b1") == 0, SUITE,
			CASE_NAME, "unexpected parsed callback input");
	GENERATED_TEST_ASSERT(strcmp(native_replace_all_ctx.input, "a1b1") == 0,
			SUITE, CASE_NAME, "unexpected native callback input");
	GENERATED_TEST_ASSERT(replace_all_ctx.offsets[0] == 1 &&
			replace_all_ctx.offsets[1] == 3, SUITE, CASE_NAME,
			"unexpected parsed callback offsets");
	GENERATED_TEST_ASSERT(native_replace_all_ctx.offsets[0] == 1 &&
			native_replace_all_ctx.offsets[1] == 3, SUITE, CASE_NAME,
			"unexpected native callback offsets");
	return generated_test_pass(SUITE, CASE_NAME);
}

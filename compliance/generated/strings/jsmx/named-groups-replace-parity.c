#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/named-groups-replace-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a1b2\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t pattern;
	jsval_t global_flags;
	jsval_t single_regex;
	jsval_t global_regex;
	jsval_t replacement;
	jsval_t missing_replacement;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 12,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a1b2", 4, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"(?<digits>[0-9])(?<tail>[a-z])?",
			sizeof("(?<digits>[0-9])(?<tail>[a-z])?") - 1, &pattern) == 0,
			SUITE, CASE_NAME, "pattern build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"g", 1, &global_flags) == 0,
			SUITE, CASE_NAME, "flags build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 0,
			jsval_undefined(), &single_regex, &error) == 0,
			SUITE, CASE_NAME, "single regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_new(&region, pattern, 1,
			global_flags, &global_regex, &error) == 0,
			SUITE, CASE_NAME, "global regex build failed");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"<$<digits>|$1|$<tail>>",
			sizeof("<$<digits>|$1|$<tail>>") - 1, &replacement) == 0,
			SUITE, CASE_NAME, "replacement build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, parsed_text,
			single_regex, replacement, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed replace failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, native_text,
			single_regex, replacement, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native replace failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a<1|1|b>2", SUITE, CASE_NAME,
			"parsed replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed replace mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "a<1|1|b>2", SUITE, CASE_NAME,
			"native replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native replace mismatch");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"$<digits>:$<tail>;",
			sizeof("$<digits>:$<tail>;") - 1, &replacement) == 0,
			SUITE, CASE_NAME, "replaceAll replacement build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all(&region, parsed_text,
			global_regex, replacement, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed replaceAll failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace_all(&region, native_text,
			global_regex, replacement, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native replaceAll failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a1:b;2:;", SUITE, CASE_NAME,
			"parsed replaceAll") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed replaceAll mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "a1:b;2:;", SUITE, CASE_NAME,
			"native replaceAll") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native replaceAll mismatch");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"$<missing>", sizeof("$<missing>") - 1,
			&missing_replacement) == 0,
			SUITE, CASE_NAME, "missing replacement build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, parsed_text,
			single_regex, missing_replacement, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed missing replace failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, native_text,
			single_regex, missing_replacement, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native missing replace failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a$<missing>2", SUITE, CASE_NAME,
			"parsed missing replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed missing replace mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "a$<missing>2", SUITE, CASE_NAME,
			"native missing replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native missing replace mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/string_replace_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/json-backed-replace-parity"

int
main(void)
{
	static const uint8_t json[] =
		"{\"text\":\"a1b2\",\"search\":\"1\",\"replacement\":\"X\"}";
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t parsed_search;
	jsval_t parsed_replacement;
	jsval_t native_text;
	jsval_t native_search;
	jsval_t native_replacement;
	jsval_t no_match;
	jsval_t regex_replacement;
	jsval_t regex;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 16,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"search", 6, &parsed_search) == 0,
			SUITE, CASE_NAME, "parsed search lookup failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"replacement", 11, &parsed_replacement) == 0,
			SUITE, CASE_NAME, "parsed replacement lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a1b2", 4, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1", 1, &native_search) == 0,
			SUITE, CASE_NAME, "native search build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"X", 1, &native_replacement) == 0,
			SUITE, CASE_NAME, "native replacement build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, parsed_text,
			parsed_search, parsed_replacement, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed string replace failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, native_text,
			native_search, native_replacement, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native string replace failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "aXb2", SUITE, CASE_NAME,
			"parsed string replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed string replace mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "aXb2", SUITE, CASE_NAME,
			"native string replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native string replace mismatch");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"9", 1, &no_match) == 0,
			SUITE, CASE_NAME, "no-match search build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, parsed_text,
			no_match, parsed_replacement, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed no-match replace failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a1b2", SUITE, CASE_NAME,
			"parsed no-match replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed no-match replace mismatch");

	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"<$1>", 4, &regex_replacement) == 0,
			SUITE, CASE_NAME, "regex replacement build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, "([0-9])", "g",
			&regex, &error) == 0, SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, parsed_text,
			regex, regex_replacement, &parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed regex replace failed");
	GENERATED_TEST_ASSERT(jsval_method_string_replace(&region, native_text,
			regex, regex_replacement, &native_result, &error) == 0,
			SUITE, CASE_NAME, "native regex replace failed");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			parsed_result, "a<1>b<2>", SUITE, CASE_NAME,
			"parsed regex replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed regex replace mismatch");
	GENERATED_TEST_ASSERT(generated_expect_jsval_utf8_string(&region,
			native_result, "a<1>b<2>", SUITE, CASE_NAME,
			"native regex replace") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native regex replace mismatch");
	return generated_test_pass(SUITE, CASE_NAME);
}

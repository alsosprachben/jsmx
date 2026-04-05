#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/regex_exec_match_helpers.h"

#define SUITE "regex"
#define CASE_NAME "jsmx/named-groups-exec-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a1b2\",\"other\":\"a2\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t parsed_other;
	jsval_t native_text;
	jsval_t native_other;
	jsval_t regex;
	jsval_t parsed_result;
	jsval_t native_result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_json_parse(&region, json, sizeof(json) - 1, 12,
			&root) == 0, SUITE, CASE_NAME, "json parse failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &parsed_text) == 0,
			SUITE, CASE_NAME, "parsed text lookup failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"other", 5, &parsed_other) == 0,
			SUITE, CASE_NAME, "parsed other lookup failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a1b2", 4, &native_text) == 0,
			SUITE, CASE_NAME, "native text build failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a2", 2, &native_other) == 0,
			SUITE, CASE_NAME, "native other build failed");
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region,
			"(?<digits>[0-9])(?<tail>[a-z])?", NULL, &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");

	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, parsed_text,
			&parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed exec failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, native_text,
			&native_result, &error) == 0,
			SUITE, CASE_NAME, "native exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region,
			parsed_result, "0", "1b", SUITE, CASE_NAME, "parsed match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed match mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			parsed_result, "digits", "1", SUITE, CASE_NAME, "parsed digits")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			parsed_result, "tail", "b", SUITE, CASE_NAME, "parsed tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed tail mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			native_result, "digits", "1", SUITE, CASE_NAME, "native digits")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			native_result, "tail", "b", SUITE, CASE_NAME, "native tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native tail mismatch");

	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, parsed_other,
			&parsed_result, &error) == 0,
			SUITE, CASE_NAME, "parsed optional exec failed");
	GENERATED_TEST_ASSERT(jsval_regexp_exec(&region, regex, native_other,
			&native_result, &error) == 0,
			SUITE, CASE_NAME, "native optional exec failed");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			parsed_result, "digits", "2", SUITE, CASE_NAME,
			"parsed optional digits") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed optional digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_undefined_prop(&region,
			parsed_result, "tail", SUITE, CASE_NAME, "parsed optional tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed optional tail mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			native_result, "digits", "2", SUITE, CASE_NAME,
			"native optional digits") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native optional digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_undefined_prop(&region,
			native_result, "tail", SUITE, CASE_NAME, "native optional tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native optional tail mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}


#include "compliance/generated/regex_core_helpers.h"
#include "compliance/generated/regex_match_all_helpers.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/named-groups-matchAll-parity"

int
main(void)
{
	static const uint8_t json[] = "{\"text\":\"a1b2\"}";
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t root;
	jsval_t parsed_text;
	jsval_t native_text;
	jsval_t regex;
	jsval_t parsed_iterator;
	jsval_t native_iterator;
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
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8_jit(&region,
			"(?<digits>[0-9])(?<tail>[a-z])?", "g", &regex, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, parsed_text, 1,
			regex, &parsed_iterator, &error) == 0,
			SUITE, CASE_NAME, "parsed matchAll failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, native_text, 1,
			regex, &native_iterator, &error) == 0,
			SUITE, CASE_NAME, "native matchAll failed");

	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			parsed_iterator, &parsed_result, SUITE, CASE_NAME, "parsed first")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed first mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			native_iterator, &native_result, SUITE, CASE_NAME, "native first")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native first mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			parsed_result, "digits", "1", SUITE, CASE_NAME, "parsed first digits")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed first digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			parsed_result, "tail", "b", SUITE, CASE_NAME, "parsed first tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed first tail mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			native_result, "digits", "1", SUITE, CASE_NAME, "native first digits")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native first digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			native_result, "tail", "b", SUITE, CASE_NAME, "native first tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native first tail mismatch");

	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			parsed_iterator, &parsed_result, SUITE, CASE_NAME, "parsed second")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed second mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			native_iterator, &native_result, SUITE, CASE_NAME, "native second")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native second mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			parsed_result, "digits", "2", SUITE, CASE_NAME,
			"parsed second digits") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed second digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_undefined_prop(&region,
			parsed_result, "tail", SUITE, CASE_NAME, "parsed second tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed second tail mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_string_prop(&region,
			native_result, "digits", "2", SUITE, CASE_NAME,
			"native second digits") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native second digits mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_groups_undefined_prop(&region,
			native_result, "tail", SUITE, CASE_NAME, "native second tail")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native second tail mismatch");

	GENERATED_TEST_ASSERT(generated_expect_match_iterator_done(&region,
			parsed_iterator, SUITE, CASE_NAME, "parsed done")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "parsed done mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_done(&region,
			native_iterator, SUITE, CASE_NAME, "native done")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "native done mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

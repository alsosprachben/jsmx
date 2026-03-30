#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "jsmx/json-backed-abstract-equality-parity"

int
main(void)
{
	static const uint8_t input[] =
		"{\"num\":1,\"flag\":true,\"text\":\"1\",\"empty\":\"\",\"nothing\":null}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t text;
	jsval_t empty;
	jsval_t nothing;
	jsval_t one_string;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored idiomatic lowering fixture for JSON-backed/native abstract
	 * equality parity.
	 */

	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME, "failed to parse input JSON");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"num", 3, &num) == 0, SUITE, CASE_NAME,
			"failed to read num");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"flag", 4, &flag) == 0, SUITE, CASE_NAME,
			"failed to read flag");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) == 0, SUITE, CASE_NAME,
			"failed to read text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"empty", 5, &empty) == 0, SUITE, CASE_NAME,
			"failed to read empty");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"nothing", 7, &nothing) == 0, SUITE, CASE_NAME,
			"failed to read nothing");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1", 1, &one_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"1\"");

	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, num,
			one_string, 1, SUITE, CASE_NAME, "parsed num == \"1\"")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for parsed num == \"1\"");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, flag,
			one_string, 1, SUITE, CASE_NAME, "parsed flag == \"1\"")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for parsed flag == \"1\"");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, text,
			jsval_number(1.0), 1, SUITE, CASE_NAME, "parsed text == 1")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for parsed text == 1");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, empty,
			jsval_number(0.0), 1, SUITE, CASE_NAME, "parsed empty == 0")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for parsed empty == 0");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, nothing,
			jsval_undefined(), 1, SUITE, CASE_NAME,
			"parsed nothing == undefined") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for parsed nothing == undefined");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region, nothing,
			jsval_number(0.0), 1, SUITE, CASE_NAME, "parsed nothing != 0")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for parsed nothing != 0");

	return generated_test_pass(SUITE, CASE_NAME);
}

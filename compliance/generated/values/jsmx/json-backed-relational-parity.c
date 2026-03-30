#include <errno.h>
#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "jsmx/json-backed-relational-parity"

int
main(void)
{
	static const uint8_t input[] =
		"{\"num\":2,\"flag\":true,\"nothing\":null,\"text\":\"1\",\"bad\":\"x\",\"left\":\"1\",\"right\":\"2\"}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t num;
	jsval_t flag;
	jsval_t nothing;
	jsval_t text;
	jsval_t bad;
	jsval_t left;
	jsval_t right;
	int result;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored idiomatic lowering fixture for JSON-backed/native
	 * relational parity and the current string/string ENOTSUP boundary.
	 */

	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME,
			"failed to parse input JSON");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"num", 3, &num) == 0, SUITE, CASE_NAME,
			"failed to read num");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"flag", 4, &flag) == 0, SUITE, CASE_NAME,
			"failed to read flag");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"nothing", 7, &nothing) == 0, SUITE, CASE_NAME,
			"failed to read nothing");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"text", 4, &text) == 0, SUITE, CASE_NAME,
			"failed to read text");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"bad", 3, &bad) == 0, SUITE, CASE_NAME,
			"failed to read bad");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"left", 4, &left) == 0, SUITE, CASE_NAME,
			"failed to read left");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"right", 5, &right) == 0, SUITE, CASE_NAME,
			"failed to read right");

	GENERATED_TEST_ASSERT(jsval_greater_than(&region, num, flag, &result) == 0,
			SUITE, CASE_NAME, "failed to lower parsed num > parsed flag");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 1, SUITE,
			CASE_NAME, "parsed num > parsed flag") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for parsed num > parsed flag");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, nothing, flag, &result) == 0,
			SUITE, CASE_NAME, "failed to lower parsed nothing < parsed flag");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 1, SUITE,
			CASE_NAME, "parsed nothing < parsed flag") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for parsed nothing < parsed flag");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, text, flag, &result) == 0,
			SUITE, CASE_NAME, "failed to lower parsed text < parsed flag");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "parsed text < parsed flag") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for parsed text < parsed flag");
	GENERATED_TEST_ASSERT(jsval_greater_equal(&region, flag, text, &result)
			== 0, SUITE, CASE_NAME,
			"failed to lower parsed flag >= parsed text");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 1, SUITE,
			CASE_NAME, "parsed flag >= parsed text") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for parsed flag >= parsed text");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, bad, jsval_number(1.0),
			&result) == 0, SUITE, CASE_NAME,
			"failed to lower parsed bad < 1");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "parsed bad < 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for parsed bad < 1");

	errno = 0;
	if (jsval_less_than(&region, left, right, &result) == 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected parsed left < parsed right to remain unsupported while both operands are strings");
	}
	GENERATED_TEST_ASSERT(errno == ENOTSUP, SUITE, CASE_NAME,
			"expected parsed string/string relational comparison to report ENOTSUP");

	return generated_test_pass(SUITE, CASE_NAME);
}

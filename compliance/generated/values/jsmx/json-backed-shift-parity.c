#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "jsmx/json-backed-shift-parity"

int
main(void)
{
	static const uint8_t input[] =
		"{\"truth\":true,\"count\":\"33\",\"neg\":-1,\"num\":5}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t count;
	jsval_t neg;
	jsval_t num;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored idiomatic lowering fixture for JSON-backed/native shift
	 * parity.
	 */

	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 12,
			&root) == 0, SUITE, CASE_NAME, "failed to parse input JSON");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"truth", 5, &truth) == 0, SUITE, CASE_NAME,
			"failed to read truth");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"count", 5, &count) == 0, SUITE, CASE_NAME,
			"failed to read count");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"neg", 3, &neg) == 0, SUITE, CASE_NAME,
			"failed to read neg");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"num", 3, &num) == 0, SUITE, CASE_NAME,
			"failed to read num");

	GENERATED_TEST_ASSERT(jsval_shift_left(&region, truth, count, &result) == 0,
			SUITE, CASE_NAME, "failed to lower parsed truth << parsed count");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 2.0, SUITE,
			CASE_NAME, "parsed truth << parsed count") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for parsed truth << parsed count");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, num, count, &result) == 0,
			SUITE, CASE_NAME, "failed to lower parsed num >> parsed count");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 2.0, SUITE,
			CASE_NAME, "parsed num >> parsed count") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for parsed num >> parsed count");
	GENERATED_TEST_ASSERT(jsval_shift_right_unsigned(&region, neg, truth, &result)
			== 0, SUITE, CASE_NAME,
			"failed to lower parsed neg >>> parsed truth");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result,
			2147483647.0, SUITE, CASE_NAME,
			"parsed neg >>> parsed truth") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for parsed neg >>> parsed truth");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "jsmx/json-backed-bitwise-parity"

int
main(void)
{
	static const uint8_t input[] =
		"{\"truth\":true,\"zero\":\"0\",\"one\":\"1\",\"bad\":\"x\",\"nothing\":null,\"num\":5}";
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t root;
	jsval_t truth;
	jsval_t one;
	jsval_t bad;
	jsval_t nothing;
	jsval_t num;
	jsval_t result;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored idiomatic lowering fixture for JSON-backed/native bitwise
	 * parity.
	 */

	GENERATED_TEST_ASSERT(jsval_json_parse(&region, input, sizeof(input) - 1, 24,
			&root) == 0, SUITE, CASE_NAME, "failed to parse input JSON");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"truth", 5, &truth) == 0, SUITE, CASE_NAME,
			"failed to read truth");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"one", 3, &one) == 0, SUITE, CASE_NAME,
			"failed to read one");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"bad", 3, &bad) == 0, SUITE, CASE_NAME,
			"failed to read bad");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"nothing", 7, &nothing) == 0, SUITE, CASE_NAME,
			"failed to read nothing");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, root,
			(const uint8_t *)"num", 3, &num) == 0, SUITE, CASE_NAME,
			"failed to read num");

	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, nothing, &result) == 0,
			SUITE, CASE_NAME, "failed to lower ~parsed nothing");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~parsed nothing") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for ~parsed nothing");
	GENERATED_TEST_ASSERT(jsval_bitwise_and(&region, truth, num, &result) == 0,
			SUITE, CASE_NAME, "failed to lower parsed truth & parsed num");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 1.0, SUITE,
			CASE_NAME, "parsed truth & parsed num") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for parsed truth & parsed num");
	GENERATED_TEST_ASSERT(jsval_bitwise_or(&region, one, jsval_bool(0), &result)
			== 0, SUITE, CASE_NAME, "failed to lower parsed one | false");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 1.0, SUITE,
			CASE_NAME, "parsed one | false") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for parsed one | false");
	GENERATED_TEST_ASSERT(jsval_bitwise_xor(&region, bad, jsval_bool(1), &result)
			== 0, SUITE, CASE_NAME, "failed to lower parsed bad ^ true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 1.0, SUITE,
			CASE_NAME, "parsed bad ^ true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for parsed bad ^ true");

	return generated_test_pass(SUITE, CASE_NAME);
}

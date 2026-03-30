#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.8.1_A4.1"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	int result;

	/*
	 * Generated from test262:
	 * test/language/expressions/less-than/S11.8.1_A4.1.js
	 *
	 * This is a direct flattened translation of the NaN-false less-than edge
	 * cases through jsval_less_than.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(0.0), &result) == 0, SUITE, CASE_NAME,
			"failed to lower NaN < 0");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < 0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN < 0");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(1.1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower NaN < 1.1");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < 1.1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN < 1.1");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(-1.1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower NaN < -1.1");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < -1.1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN < -1.1");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(NAN), &result) == 0, SUITE, CASE_NAME,
			"failed to lower NaN < NaN");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < NaN") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN < NaN");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(INFINITY), &result) == 0, SUITE, CASE_NAME,
			"failed to lower NaN < +Infinity");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < +Infinity") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN < +Infinity");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(-INFINITY), &result) == 0, SUITE, CASE_NAME,
			"failed to lower NaN < -Infinity");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < -Infinity") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for NaN < -Infinity");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(1.7976931348623157e+308), &result) == 0,
			SUITE, CASE_NAME, "failed to lower NaN < Number.MAX_VALUE");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < Number.MAX_VALUE") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for NaN < Number.MAX_VALUE");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, jsval_number(NAN),
			jsval_number(5e-324), &result) == 0,
			SUITE, CASE_NAME, "failed to lower NaN < Number.MIN_VALUE");
	GENERATED_TEST_ASSERT(generated_expect_boolean_result(result, 0, SUITE,
			CASE_NAME, "NaN < Number.MIN_VALUE") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for NaN < Number.MIN_VALUE");

	return generated_test_pass(SUITE, CASE_NAME);
}

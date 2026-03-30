#include <math.h>
#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.8_A3_T2"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/bitwise-not/S11.4.8_A3_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive number
	 * checks. The Number-object checks in CHECK#2, CHECK#4, and CHECK#6 are
	 * intentionally omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_number(0.1), &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~0.1");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~0.1") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~0.1");
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_number(NAN), &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~NaN");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~NaN") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~NaN");
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_number(1.0), &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~1");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -2.0, SUITE,
			CASE_NAME, "~1") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~1");
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_number(INFINITY),
			&result) == 0, SUITE, CASE_NAME, "failed to lower ~Infinity");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~Infinity") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~Infinity");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <float.h>
#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.2_A4_T4"

int
main(void)
{
	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S9.2_A4_T4.js
	 *
	 * This is a direct flattened translation of logical-not over truthy
	 * numeric operands.
	 */

	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(INFINITY))) == 0,
			SUITE, CASE_NAME, "expected !(+Infinity) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(-INFINITY))) == 0,
			SUITE, CASE_NAME, "expected !(-Infinity) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(DBL_MAX))) == 0,
			SUITE, CASE_NAME, "expected !(Number.MAX_VALUE) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(DBL_MIN))) == 0,
			SUITE, CASE_NAME, "expected !(Number.MIN_VALUE) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(13.0))) == 0,
			SUITE, CASE_NAME, "expected !(13) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(-13.0))) == 0,
			SUITE, CASE_NAME, "expected !(-13) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(1.3))) == 0,
			SUITE, CASE_NAME, "expected !(1.3) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(-1.3))) == 0,
			SUITE, CASE_NAME, "expected !(-1.3) to be false");

	return generated_test_pass(SUITE, CASE_NAME);
}

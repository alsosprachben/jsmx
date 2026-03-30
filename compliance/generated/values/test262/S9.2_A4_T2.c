#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.2_A4_T2"

int
main(void)
{
	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S9.2_A4_T2.js
	 *
	 * This is a direct flattened translation of logical-not over the falsy
	 * numeric edge cases +0, -0, and NaN.
	 */

	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(+0.0))) == 1,
			SUITE, CASE_NAME, "expected !(+0) to be true");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(-0.0))) == 1,
			SUITE, CASE_NAME, "expected !(-0) to be true");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_number(NAN))) == 1,
			SUITE, CASE_NAME, "expected !(NaN) to be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

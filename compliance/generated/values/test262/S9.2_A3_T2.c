#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.2_A3_T2"

int
main(void)
{
	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S9.2_A3_T2.js
	 *
	 * This is a direct flattened translation of logical-not over primitive
	 * boolean operands.
	 */

	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_bool(1))) == 0,
			SUITE, CASE_NAME, "expected !(true) to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_bool(0))) == 1,
			SUITE, CASE_NAME, "expected !(false) to be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

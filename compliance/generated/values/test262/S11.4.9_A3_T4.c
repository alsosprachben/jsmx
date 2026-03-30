#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.9_A3_T4"

int
main(void)
{
	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S11.4.9_A3_T4.js
	 *
	 * This is a direct flattened translation of logical-not over undefined
	 * and null through jsval_truthy.
	 */

	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_undefined())) == 1,
			SUITE, CASE_NAME, "expected !undefined to be true");
	GENERATED_TEST_ASSERT((!jsval_truthy(NULL, jsval_null())) == 1,
			SUITE, CASE_NAME, "expected !null to be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

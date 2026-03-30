#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.4_A4.2"

int
main(void)
{
	uint8_t storage[128];
	jsval_region_t region;

	/*
	 * Generated from test262:
	 * test/language/expressions/strict-equals/S11.9.4_A4.2.js
	 *
	 * This is a direct flattened translation of the +0 / -0 strict-equality
	 * edge case through jsval_strict_eq.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(+0.0),
			jsval_number(-0.0)) == 1, SUITE, CASE_NAME,
			"expected +0 === -0 to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(-0.0),
			jsval_number(+0.0)) == 1, SUITE, CASE_NAME,
			"expected -0 === +0 to be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

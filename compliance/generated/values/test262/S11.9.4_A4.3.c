#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.4_A4.3"

int
main(void)
{
	uint8_t storage[128];
	jsval_region_t region;

	/*
	 * Generated from test262:
	 * test/language/expressions/strict-equals/S11.9.4_A4.3.js
	 *
	 * This is a direct flattened translation of the primitive-number strict
	 * equality cases from the upstream file.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(INFINITY),
			jsval_number(INFINITY)) == 1, SUITE, CASE_NAME,
			"expected +Infinity === +Infinity to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(-INFINITY),
			jsval_number(-INFINITY)) == 1, SUITE, CASE_NAME,
			"expected -Infinity === -Infinity to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(13.0),
			jsval_number(13.0)) == 1, SUITE, CASE_NAME,
			"expected 13 === 13 to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(-13.0),
			jsval_number(-13.0)) == 1, SUITE, CASE_NAME,
			"expected -13 === -13 to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(1.3),
			jsval_number(1.3)) == 1, SUITE, CASE_NAME,
			"expected 1.3 === 1.3 to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(-1.3),
			jsval_number(-1.3)) == 1, SUITE, CASE_NAME,
			"expected -1.3 === -1.3 to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(INFINITY),
			jsval_number(-(-INFINITY))) == 1, SUITE, CASE_NAME,
			"expected +Infinity === -(-Infinity) to be true");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(1.0),
			jsval_number(0.999999999999)) == 0, SUITE, CASE_NAME,
			"expected 1 === 0.999999999999 to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(1.0),
			jsval_number(1.0)) == 1, SUITE, CASE_NAME,
			"expected 1.0 === 1 to be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

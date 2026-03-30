#include <float.h>
#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.4_A4.1_T1"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t text;

	/*
	 * Generated from test262:
	 * test/language/expressions/strict-equals/S11.9.4_A4.1_T1.js
	 *
	 * This idiomatic flattened translation preserves the primitive NaN checks.
	 * The object comparison in CHECK#9 is intentionally omitted because object
	 * identity is above this primitive-only strict-equality slice.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"string", 6, &text) == 0,
			SUITE, CASE_NAME, "failed to construct string operand");

	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN), jsval_bool(1)) == 0,
			SUITE, CASE_NAME, "expected NaN === true to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN), jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "expected NaN === 1 to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN), jsval_number(NAN)) == 0,
			SUITE, CASE_NAME, "expected NaN === NaN to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN),
			jsval_number(INFINITY)) == 0, SUITE, CASE_NAME,
			"expected NaN === +Infinity to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN),
			jsval_number(-INFINITY)) == 0, SUITE, CASE_NAME,
			"expected NaN === -Infinity to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN),
			jsval_number(DBL_MAX)) == 0, SUITE, CASE_NAME,
			"expected NaN === Number.MAX_VALUE to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN),
			jsval_number(DBL_MIN)) == 0, SUITE, CASE_NAME,
			"expected NaN === Number.MIN_VALUE to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN), text) == 0,
			SUITE, CASE_NAME, "expected NaN === \"string\" to be false");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.11.2_A4_T2"

int
main(void)
{
	uint8_t storage[128];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-or/S11.11.2_A4_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive numeric
	 * truthy-left cases. The Number-object cases are intentionally omitted
	 * above the current flattened `jsval` layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	result = generated_logical_or(&region, jsval_number(-1.0),
			jsval_number(1.0));
	GENERATED_TEST_ASSERT(generated_expect_value_strict_eq(&region, result,
			jsval_number(-1.0), SUITE, CASE_NAME,
			"-1 || 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -1 || 1");

	result = generated_logical_or(&region, jsval_number(-1.0),
			jsval_number(NAN));
	GENERATED_TEST_ASSERT(generated_expect_value_strict_eq(&region, result,
			jsval_number(-1.0), SUITE, CASE_NAME,
			"-1 || NaN") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for -1 || NaN");

	return generated_test_pass(SUITE, CASE_NAME);
}

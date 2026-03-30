#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.5.2_A3_T1.1"

int
main(void)
{
	uint8_t storage[64];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/division/S11.5.2_A3_T1.1.js
	 *
	 * This idiomatic flattened translation preserves the primitive boolean
	 * division case. The wrapper-object cases are intentionally omitted at the
	 * current flattened layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_divide(&region, jsval_bool(1), jsval_bool(1),
			&result) == 0, SUITE, CASE_NAME,
			"failed to lower true / true");
	GENERATED_TEST_ASSERT(generated_expect_value_strict_eq(&region, result,
			jsval_number(1.0), SUITE, CASE_NAME,
			"true / true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for true / true");

	return generated_test_pass(SUITE, CASE_NAME);
}

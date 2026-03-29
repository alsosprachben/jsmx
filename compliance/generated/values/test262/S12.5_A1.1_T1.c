#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S12.5_A1.1_T1"

int
main(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t empty_string;

	/*
	 * Generated from test262:
	 * test/language/statements/if/S12.5_A1.1_T1.js
	 *
	 * This is an idiomatic flattened translation of ToBoolean through
	 * `jsval_truthy` over the primitive falsy cases from the upstream file.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty_string) == 0, SUITE, CASE_NAME,
			"failed to construct empty string operand");

	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_number(0.0)) == 0,
			SUITE, CASE_NAME, "expected 0 to be falsy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_bool(0)) == 0,
			SUITE, CASE_NAME, "expected false to be falsy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_null()) == 0,
			SUITE, CASE_NAME, "expected null to be falsy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_undefined()) == 0,
			SUITE, CASE_NAME, "expected undefined to be falsy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, empty_string) == 0,
			SUITE, CASE_NAME, "expected empty string to be falsy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_number(NAN)) == 0,
			SUITE, CASE_NAME, "expected NaN to be falsy");

	return generated_test_pass(SUITE, CASE_NAME);
}

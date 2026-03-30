#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.2_A5_T4"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t nonempty;
	jsval_t space;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S9.2_A5_T4.js
	 *
	 * This is a direct flattened translation of logical-not over nonempty
	 * string truthiness.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)" ", 1,
			&space) == 0, SUITE, CASE_NAME,
			"failed to construct single-space string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"Nonempty String", 15, &nonempty) == 0,
			SUITE, CASE_NAME, "failed to construct nonempty string");
	GENERATED_TEST_ASSERT((!jsval_truthy(&region, space)) == 0,
			SUITE, CASE_NAME, "expected !(\" \") to be false");
	GENERATED_TEST_ASSERT((!jsval_truthy(&region, nonempty)) == 0,
			SUITE, CASE_NAME, "expected !(\"Nonempty String\") to be false");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S12.5_A1_T2"

int
main(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t object;
	jsval_t array;

	/*
	 * Generated from test262:
	 * test/language/statements/if/S12.5_A1_T2.js
	 *
	 * This idiomatic flattened translation preserves the truthiness contract
	 * for object and array values through direct native jsval containers.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_object_new(&region, 0, &object) == 0,
			SUITE, CASE_NAME, "failed to allocate object operand");
	GENERATED_TEST_ASSERT(jsval_array_new(&region, 0, &array) == 0,
			SUITE, CASE_NAME, "failed to allocate array operand");

	GENERATED_TEST_ASSERT(jsval_truthy(&region, object) == 1,
			SUITE, CASE_NAME, "expected object values to be truthy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, array) == 1,
			SUITE, CASE_NAME, "expected array values to be truthy");

	return generated_test_pass(SUITE, CASE_NAME);
}

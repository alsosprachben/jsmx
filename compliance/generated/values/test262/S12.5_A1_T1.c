#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S12.5_A1_T1"

int
main(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t one_string;
	jsval_t a_string;

	/*
	 * Generated from test262:
	 * test/language/statements/if/S12.5_A1_T1.js
	 *
	 * This is an idiomatic flattened translation of ToBoolean through
	 * `jsval_truthy` over the primitive cases from the upstream file.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"1\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"A", 1,
			&a_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"A\"");

	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_number(1.0)) == 1,
			SUITE, CASE_NAME, "expected 1 to be truthy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, jsval_bool(1)) == 1,
			SUITE, CASE_NAME, "expected true to be truthy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, one_string) == 1,
			SUITE, CASE_NAME, "expected \"1\" to be truthy");
	GENERATED_TEST_ASSERT(jsval_truthy(&region, a_string) == 1,
			SUITE, CASE_NAME, "expected \"A\" to be truthy");

	return generated_test_pass(SUITE, CASE_NAME);
}

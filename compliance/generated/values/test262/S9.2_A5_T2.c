#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S9.2_A5_T2"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t empty_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-not/S9.2_A5_T2.js
	 *
	 * This is a direct flattened translation of logical-not over the empty
	 * string falsy case.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty_string) == 0, SUITE, CASE_NAME,
			"failed to construct empty string operand");
	GENERATED_TEST_ASSERT((!jsval_truthy(&region, empty_string)) == 1,
			SUITE, CASE_NAME, "expected !(\"\") to be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

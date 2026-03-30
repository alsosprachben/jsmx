#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.11.2_A3_T3"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t empty_string;
	jsval_t one_string;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-or/S11.11.2_A3_T3.js
	 *
	 * This idiomatic flattened translation preserves the primitive string
	 * case. The String-object case is intentionally omitted above the current
	 * flattened `jsval` layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"", 0,
			&empty_string) == 0, SUITE, CASE_NAME,
			"failed to construct empty string operand");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct \"1\" string operand");

	result = generated_logical_or(&region, empty_string, one_string);
	GENERATED_TEST_ASSERT(generated_expect_value_string(&region, result,
			(const uint8_t *)"1", 1, SUITE, CASE_NAME,
			"\"\" || \"1\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"\" || \"1\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

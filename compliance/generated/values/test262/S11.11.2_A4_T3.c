#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.11.2_A4_T3"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t minus_one_string;
	jsval_t one_string;
	jsval_t x_string;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/logical-or/S11.11.2_A4_T3.js
	 *
	 * This idiomatic flattened translation preserves the primitive string
	 * truthy-left cases. The String-object cases are intentionally omitted
	 * above the current flattened `jsval` layer.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"-1", 2, &minus_one_string) == 0,
			SUITE, CASE_NAME, "failed to construct \"-1\" string operand");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct \"1\" string operand");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"x", 1,
			&x_string) == 0, SUITE, CASE_NAME,
			"failed to construct \"x\" string operand");

	result = generated_logical_or(&region, minus_one_string, one_string);
	GENERATED_TEST_ASSERT(generated_expect_value_string(&region, result,
			(const uint8_t *)"-1", 2, SUITE, CASE_NAME,
			"\"-1\" || \"1\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"-1\" || \"1\"");

	result = generated_logical_or(&region, minus_one_string, x_string);
	GENERATED_TEST_ASSERT(generated_expect_value_string(&region, result,
			(const uint8_t *)"-1", 2, SUITE, CASE_NAME,
			"\"-1\" || \"x\"") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "unexpected result for \"-1\" || \"x\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

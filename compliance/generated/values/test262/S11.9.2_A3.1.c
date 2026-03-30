#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.2_A3.1"

int
main(void)
{
	uint8_t storage[512];
	jsval_region_t region;

	/*
	 * Generated from test262:
	 * test/language/expressions/does-not-equals/S11.9.2_A3.1.js
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region, jsval_bool(1),
			jsval_bool(1), 0, SUITE, CASE_NAME, "true != true")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for true != true");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region, jsval_bool(0),
			jsval_bool(0), 0, SUITE, CASE_NAME, "false != false")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for false != false");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region, jsval_bool(1),
			jsval_bool(0), 1, SUITE, CASE_NAME, "true != false")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for true != false");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region, jsval_bool(0),
			jsval_bool(1), 1, SUITE, CASE_NAME, "false != true")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for false != true");

	return generated_test_pass(SUITE, CASE_NAME);
}

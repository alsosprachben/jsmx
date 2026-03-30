#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.2_A4.2"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;

	/*
	 * Generated from test262:
	 * test/language/expressions/does-not-equals/S11.9.2_A4.2.js
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(0.0), jsval_number(-0.0), 0, SUITE, CASE_NAME,
			"+0 != -0") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for +0 != -0");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(-0.0), jsval_number(0.0), 0, SUITE, CASE_NAME,
			"-0 != +0") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for -0 != +0");

	return generated_test_pass(SUITE, CASE_NAME);
}

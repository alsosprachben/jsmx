#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.1_A3.3"

int
main(void)
{
	uint8_t storage[512];
	jsval_region_t region;
	jsval_t one_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/equals/S11.9.1_A3.3.js
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"1", 1, &one_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"1\"");

	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_number(0.0), jsval_bool(0), 1, SUITE, CASE_NAME,
			"0 == false") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for 0 == false");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, one_string,
			jsval_bool(1), 1, SUITE, CASE_NAME, "\"1\" == true")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for \"1\" == true");

	return generated_test_pass(SUITE, CASE_NAME);
}

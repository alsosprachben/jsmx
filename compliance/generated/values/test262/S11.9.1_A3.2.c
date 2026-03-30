#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.1_A3.2"

int
main(void)
{
	uint8_t storage[512];
	jsval_region_t region;
	jsval_t zero_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/equals/S11.9.1_A3.2.js
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"0", 1, &zero_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"0\"");

	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, jsval_bool(1),
			jsval_number(1.0), 1, SUITE, CASE_NAME, "true == 1")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for true == 1");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, jsval_bool(0),
			zero_string, 1, SUITE, CASE_NAME, "false == \"0\"")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for false == \"0\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

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
	jsval_t a_string;
	jsval_t one_string;
	int c = 0;

	/*
	 * Generated from test262:
	 * test/language/statements/if/S12.5_A1_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive truthiness
	 * checks from the upstream if/else file, including the explicit else-path
	 * counter updates.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"1\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"A", 1,
			&a_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"A\"");

	if (!jsval_truthy(&region, jsval_number(1.0))) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected 1 to be truthy in if/else");
	} else {
		c++;
	}
	GENERATED_TEST_ASSERT(c == 1, SUITE, CASE_NAME,
			"expected else counter to reach 1 after numeric truthy check");

	if (!jsval_truthy(&region, jsval_bool(1))) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected true to be truthy in if/else");
	} else {
		c++;
	}
	GENERATED_TEST_ASSERT(c == 2, SUITE, CASE_NAME,
			"expected else counter to reach 2 after boolean truthy check");

	if (!jsval_truthy(&region, one_string)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected \"1\" to be truthy in if/else");
	} else {
		c++;
	}
	GENERATED_TEST_ASSERT(c == 3, SUITE, CASE_NAME,
			"expected else counter to reach 3 after string truthy check");

	if (!jsval_truthy(&region, a_string)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected \"A\" to be truthy in if/else");
	} else {
		c++;
	}
	GENERATED_TEST_ASSERT(c == 4, SUITE, CASE_NAME,
			"expected else counter to reach 4 after final truthy check");

	return generated_test_pass(SUITE, CASE_NAME);
}

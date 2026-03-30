#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.4_A8_T2"

int
main(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t zero_string;
	jsval_t one_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/strict-equals/S11.9.4_A8_T2.js
	 *
	 * This idiomatic flattened translation preserves the primitive
	 * cross-type checks. Wrapper-object and object-valueOf cases are
	 * intentionally omitted because they are outside the current flattened
	 * `jsval` value surface.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"1\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"0", 1,
			&zero_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"0\"");

	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(1.0),
			jsval_bool(1)) == 0, SUITE, CASE_NAME,
			"expected 1 === true to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(1.0),
			one_string) == 0, SUITE, CASE_NAME,
			"expected 1 === \"1\" to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_bool(0),
			jsval_number(0.0)) == 0, SUITE, CASE_NAME,
			"expected false === 0 to be false");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, zero_string,
			jsval_number(0.0)) == 0, SUITE, CASE_NAME,
			"expected \"0\" === 0 to be false");

	return generated_test_pass(SUITE, CASE_NAME);
}

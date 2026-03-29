#include <math.h>
#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/strict-eq-primitives"

int
main(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t same_string;
	jsval_t same_string_again;
	jsval_t one_string;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4,
			&same_string) == 0, SUITE, CASE_NAME,
			"failed to construct first string operand");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"same", 4,
			&same_string_again) == 0, SUITE, CASE_NAME,
			"failed to construct second string operand");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region, (const uint8_t *)"1", 1,
			&one_string) == 0, SUITE, CASE_NAME,
			"failed to construct numeric string operand");

	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_undefined(),
			jsval_undefined()) == 1, SUITE, CASE_NAME,
			"expected undefined to strictly equal itself");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_null(),
			jsval_null()) == 1, SUITE, CASE_NAME,
			"expected null to strictly equal itself");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(+0.0),
			jsval_number(-0.0)) == 1, SUITE, CASE_NAME,
			"expected +0 and -0 to compare strictly equal");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, same_string,
			same_string_again) == 1, SUITE, CASE_NAME,
			"expected equal primitive strings to compare strictly equal");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(NAN),
			jsval_number(NAN)) == 0, SUITE, CASE_NAME,
			"expected NaN to fail strict equality with itself");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(1.0),
			one_string) == 0, SUITE, CASE_NAME,
			"expected number and string to fail strict equality");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_bool(1),
			jsval_number(1.0)) == 0, SUITE, CASE_NAME,
			"expected boolean and number to fail strict equality");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_null(),
			jsval_undefined()) == 0, SUITE, CASE_NAME,
			"expected null and undefined to fail strict equality");

	return generated_test_pass(SUITE, CASE_NAME);
}

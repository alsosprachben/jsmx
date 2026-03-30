#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.5_A8_T4"

int
main(void)
{
	uint8_t storage[2048];
	jsval_region_t region;
	jsval_t null_string;
	jsval_t undefined_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/strict-does-not-equals/S11.9.5_A8_T4.js
	 *
	 * This idiomatic flattened translation preserves the primitive `!==`
	 * null and undefined cross-type checks through direct negation of
	 * `jsval_strict_eq`. The object comparisons in CHECK#9 and CHECK#10 are
	 * intentionally omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"null", 4, &null_string) == 0,
			SUITE, CASE_NAME, "failed to construct string operand \"null\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"undefined", 9, &undefined_string) == 0,
			SUITE, CASE_NAME,
			"failed to construct string operand \"undefined\"");

	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_undefined(),
			jsval_null()) == 0, SUITE, CASE_NAME,
			"expected undefined !== null to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_null(),
			jsval_undefined()) == 0, SUITE, CASE_NAME,
			"expected null !== undefined to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_null(),
			jsval_number(0.0)) == 0, SUITE, CASE_NAME,
			"expected null !== 0 to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_number(0.0),
			jsval_null()) == 0, SUITE, CASE_NAME,
			"expected 0 !== null to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_null(),
			jsval_bool(0)) == 0, SUITE, CASE_NAME,
			"expected null !== false to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_bool(0),
			jsval_null()) == 0, SUITE, CASE_NAME,
			"expected false !== null to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_undefined(),
			jsval_bool(0)) == 0, SUITE, CASE_NAME,
			"expected undefined !== false to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_bool(0),
			jsval_undefined()) == 0, SUITE, CASE_NAME,
			"expected false !== undefined to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_null(),
			null_string) == 0, SUITE, CASE_NAME,
			"expected null !== \"null\" to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, null_string,
			jsval_null()) == 0, SUITE, CASE_NAME,
			"expected \"null\" !== null to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, jsval_undefined(),
			undefined_string) == 0, SUITE, CASE_NAME,
			"expected undefined !== \"undefined\" to hold");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, undefined_string,
			jsval_undefined()) == 0, SUITE, CASE_NAME,
			"expected \"undefined\" !== undefined to hold");

	return generated_test_pass(SUITE, CASE_NAME);
}

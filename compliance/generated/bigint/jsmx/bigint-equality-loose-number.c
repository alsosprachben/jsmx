#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "bigint"
#define CASE_NAME "jsmx/bigint-equality-loose-number"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t forty_two;
	jsval_t forty_two_string;
	jsval_t forty_two_point_five_string;
	jsval_t result;
	int equal = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_bigint_new_i64(&region, 42, &forty_two) == 0 &&
			jsval_string_new_utf8(&region,
				(const uint8_t *)"42", 2, &forty_two_string) == 0 &&
			jsval_string_new_utf8(&region,
				(const uint8_t *)"42.5", 4,
				&forty_two_point_five_string) == 0,
			SUITE, CASE_NAME, "construct values");

	/* 42n == 42.0: loose equality across Number/BigInt. */
	GENERATED_TEST_ASSERT(
			jsval_abstract_eq(&region, forty_two,
				jsval_number(42.0), &equal) == 0 && equal == 1,
			SUITE, CASE_NAME,
			"42n == 42.0 should be true (loose)");

	/* 42n == 42.5: false. */
	GENERATED_TEST_ASSERT(
			jsval_abstract_eq(&region, forty_two,
				jsval_number(42.5), &equal) == 0 && equal == 0,
			SUITE, CASE_NAME, "42n == 42.5 should be false");

	/* 42n == "42": string coerces to equal numeric value. */
	GENERATED_TEST_ASSERT(
			jsval_abstract_eq(&region, forty_two, forty_two_string,
				&equal) == 0 && equal == 1,
			SUITE, CASE_NAME, "42n == '42' should be true");

	/* 42n == "42.5": string coerces but not equal. */
	GENERATED_TEST_ASSERT(
			jsval_abstract_eq(&region, forty_two,
				forty_two_point_five_string, &equal) == 0 &&
			equal == 0,
			SUITE, CASE_NAME, "42n == '42.5' should be false");

	/* 42n === 42.0: strict equality distinguishes types. */
	GENERATED_TEST_ASSERT(
			jsval_strict_eq(&region, forty_two,
				jsval_number(42.0)) == 0,
			SUITE, CASE_NAME,
			"42n === 42.0 should be false (strict)");

	/* BigInt + Number is not allowed: jsval_add -> -1 / ENOTSUP. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_add(&region, forty_two, jsval_number(1.0),
				&result) < 0 && errno == ENOTSUP,
			SUITE, CASE_NAME,
			"BigInt + Number should fail with ENOTSUP");

	/* BigInt - Number also fails. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_subtract(&region, forty_two, jsval_number(1.0),
				&result) < 0 && errno == ENOTSUP,
			SUITE, CASE_NAME,
			"BigInt - Number should fail with ENOTSUP");

	/* BigInt * Number also fails. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_multiply(&region, forty_two, jsval_number(2.0),
				&result) < 0 && errno == ENOTSUP,
			SUITE, CASE_NAME,
			"BigInt * Number should fail with ENOTSUP");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "bigint"
#define CASE_NAME "jsmx/bigint-compare-ordering"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t zero;
	jsval_t forty_two;
	jsval_t forty_two_text;
	jsval_t big_positive;
	jsval_t big_negative;
	int cmp = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_bigint_new_i64(&region, 0, &zero) == 0 &&
			jsval_bigint_new_i64(&region, 42, &forty_two) == 0 &&
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"42", 2, &forty_two_text) == 0 &&
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"12345678901234567890", 20,
				&big_positive) == 0 &&
			jsval_bigint_new_utf8(&region,
				(const uint8_t *)"-12345678901234567890", 21,
				&big_negative) == 0,
			SUITE, CASE_NAME, "construct values");

	/* zero < 42: negative sign. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_compare(&region, zero, forty_two, &cmp) == 0 &&
			cmp < 0,
			SUITE, CASE_NAME, "compare(0, 42) should be < 0");

	/* 42 > zero: positive sign. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_compare(&region, forty_two, zero, &cmp) == 0 &&
			cmp > 0,
			SUITE, CASE_NAME, "compare(42, 0) should be > 0");

	/* 42 == parse("42"): zero. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_compare(&region, forty_two, forty_two_text,
				&cmp) == 0 && cmp == 0,
			SUITE, CASE_NAME,
			"compare(42, parse('42')) should be == 0");

	/* big_negative < big_positive across sign boundary,
	 * both values past 2^63 (outside Number safe range). */
	GENERATED_TEST_ASSERT(
			jsval_bigint_compare(&region, big_negative, big_positive,
				&cmp) == 0 && cmp < 0,
			SUITE, CASE_NAME,
			"compare(big-, big+) should be < 0");
	GENERATED_TEST_ASSERT(
			jsval_bigint_compare(&region, big_positive, big_negative,
				&cmp) == 0 && cmp > 0,
			SUITE, CASE_NAME,
			"compare(big+, big-) should be > 0");

	/* Reflexive: compare(big, big) == 0. */
	GENERATED_TEST_ASSERT(
			jsval_bigint_compare(&region, big_positive, big_positive,
				&cmp) == 0 && cmp == 0,
			SUITE, CASE_NAME, "reflexive compare should be 0");

	return generated_test_pass(SUITE, CASE_NAME);
}

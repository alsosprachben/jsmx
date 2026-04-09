#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "values"
#define CASE_NAME "jsmx/bigint-equality-relational-parity"

int
main(void)
{
	uint8_t storage[4096];
	jsval_region_t region;
	jsval_t answer;
	jsval_t hundred;
	jsval_t text_42;
	jsval_t text_42_5;
	int result = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bigint_new_i64(&region, 42, &answer) == 0,
			SUITE, CASE_NAME, "failed to allocate answer bigint");
	GENERATED_TEST_ASSERT(jsval_bigint_new_i64(&region, 100, &hundred) == 0,
			SUITE, CASE_NAME, "failed to allocate hundred bigint");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"42", 2, &text_42) == 0,
			SUITE, CASE_NAME, "failed to allocate integral decimal string");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"42.5", 4, &text_42_5) == 0,
			SUITE, CASE_NAME, "failed to allocate fractional decimal string");

	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, answer, jsval_number(42.0),
			&result) == 0 && result == 1,
			SUITE, CASE_NAME,
			"expected bigint to stay abstract-equal to an exact integral number");
	GENERATED_TEST_ASSERT(jsval_abstract_eq(&region, answer,
			jsval_number(42.5), &result) == 0 && result == 0,
			SUITE, CASE_NAME,
			"expected bigint to stay abstract-inequal to a fractional number");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, answer, jsval_number(42.5),
			&result) == 0 && result == 1,
			SUITE, CASE_NAME,
			"expected bigint < fractional number to preserve mathematical ordering");
	GENERATED_TEST_ASSERT(jsval_greater_than(&region, hundred, text_42,
			&result) == 0 && result == 1,
			SUITE, CASE_NAME,
			"expected bigint > integral decimal string to preserve mathematical ordering");
	GENERATED_TEST_ASSERT(jsval_less_equal(&region, answer, text_42,
			&result) == 0 && result == 1,
			SUITE, CASE_NAME,
			"expected bigint <= identical integral decimal string to stay true");
	GENERATED_TEST_ASSERT(jsval_less_than(&region, answer, text_42_5,
			&result) == 0 && result == 0,
			SUITE, CASE_NAME,
			"expected bigint < fractional decimal string to stay unordered");
	GENERATED_TEST_ASSERT(jsval_greater_than(&region, answer, text_42_5,
			&result) == 0 && result == 0,
			SUITE, CASE_NAME,
			"expected bigint > fractional decimal string to stay unordered");

	return generated_test_pass(SUITE, CASE_NAME);
}

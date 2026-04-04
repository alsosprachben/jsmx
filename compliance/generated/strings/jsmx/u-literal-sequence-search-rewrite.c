#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-sequence-search-rewrite"

int
main(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'B', 'C'
	};
	static const uint16_t pair_subject_units[] = {
		'A', 0xD834, 0xDF06, '!', 'X', 0xD834, 0xDF06, '!', 'Y'
	};
	static const uint16_t boundary_subject_units[] = {
		'A', 0xD834, 0xDF06, '!'
	};
	static const uint16_t low_b_units[] = {0xDF06, 'B'};
	static const uint16_t pair_bang_units[] = {0xD834, 0xDF06, '!'};
	static const uint16_t a_high_units[] = {'A', 0xD834};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t pair_subject;
	jsval_t boundary_subject;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0,
			SUITE, CASE_NAME, "failed to build low-sequence subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_subject_units,
			sizeof(pair_subject_units) / sizeof(pair_subject_units[0]),
			&pair_subject) == 0,
			SUITE, CASE_NAME, "failed to build pair-sequence subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region,
			boundary_subject_units,
			sizeof(boundary_subject_units) /
			sizeof(boundary_subject_units[0]), &boundary_subject) == 0,
			SUITE, CASE_NAME, "failed to build boundary subject");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_sequence(
			&region, low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]),
			&result, &error) == 0,
			SUITE, CASE_NAME, "low-sequence search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "low-sequence search should return index 3");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_sequence(
			&region, pair_subject, pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]),
			&result, &error) == 0,
			SUITE, CASE_NAME, "pair-sequence search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(1.0)) == 1,
			SUITE, CASE_NAME, "pair-sequence search should return index 1");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_sequence(
			&region, boundary_subject, a_high_units,
			sizeof(a_high_units) / sizeof(a_high_units[0]),
			&result, &error) == 0,
			SUITE, CASE_NAME, "boundary search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(-1.0)) == 1,
			SUITE, CASE_NAME, "boundary-splitting pattern must not match");

	return generated_test_pass(SUITE, CASE_NAME);
}

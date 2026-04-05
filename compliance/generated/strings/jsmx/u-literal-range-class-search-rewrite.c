#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-range-class-search-rewrite"

int
main(void)
{
	static const uint16_t range_subject_units[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t pair_only_units[] = {
		'A', 0xD834, 0xDF06, '!'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_range[] = {0xD834, 0xD834};
	static const uint16_t surrogate_range[] = {0xD800, 0xDFFF};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t pair_only;
	jsval_t high_subject;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0,
			SUITE, CASE_NAME, "failed to build range subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0,
			SUITE, CASE_NAME, "failed to build pair-only subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high-subject");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &result,
			&error) == 0,
			SUITE, CASE_NAME, "range search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "expected first range search hit at 3");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_range_class(
			&region, pair_only, surrogate_range,
			sizeof(surrogate_range) / (2 * sizeof(surrogate_range[0])),
			&result, &error) == 0,
			SUITE, CASE_NAME, "pair-only range search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(-1.0)) == 1,
			SUITE, CASE_NAME, "range search should not match inside a pair");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_range_class(
			&region, high_subject, high_range,
			sizeof(high_range) / (2 * sizeof(high_range[0])),
			&result, &error) == 0,
			SUITE, CASE_NAME, "isolated high-surrogate range search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "expected isolated high-surrogate hit at 3");

	return generated_test_pass(SUITE, CASE_NAME);
}

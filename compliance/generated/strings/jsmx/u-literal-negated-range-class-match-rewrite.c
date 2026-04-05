#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-negated-range-class-match-rewrite"

int
main(void)
{
	static const uint16_t range_subject_units[] = {
		0xD834, 0xDF06, 'A', 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t high_subject_units[] = {
		0xD834, 0xDF06, 0xD834, 'C', 'B'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_and_c_ranges[] = {
		0xD834, 0xD834, 'C', 'C'
	};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t z_unit[] = {'Z'};
	static const uint16_t b_unit[] = {'B'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t high_subject;
	jsval_t expected_value;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0,
			SUITE, CASE_NAME, "failed to build negated range subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0, &result,
			&error) == 0,
			SUITE, CASE_NAME, "non-global negated range match failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch negated range match[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, a_unit, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected first negated range match to be A");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, &result,
			&error) == 0,
			SUITE, CASE_NAME, "global negated range match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two global negated range matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch global negated range match[1]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, z_unit, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected second global negated range match to be Z");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_negated_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &iterator,
			&error) == 0,
			SUITE, CASE_NAME, "negated range matchAll failed");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected first negated range matchAll result");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected second negated range matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch second negated range matchAll capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, z_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected second negated range matchAll capture to be Z");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "expected exhausted negated range matchAll iterator");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_negated_range_class(
			&region, high_subject, high_and_c_ranges,
			sizeof(high_and_c_ranges) /
			(2 * sizeof(high_and_c_ranges[0])), 0, &result,
			&error) == 0,
			SUITE, CASE_NAME, "isolated high negated range match failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch isolated high negated range capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, b_unit, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected isolated high negated range capture to be B");

	return generated_test_pass(SUITE, CASE_NAME);
}

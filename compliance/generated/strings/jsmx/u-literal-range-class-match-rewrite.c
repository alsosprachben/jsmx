#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-range-class-match-rewrite"

int
main(void)
{
	static const uint16_t range_subject_units[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t high_range[] = {0xD834, 0xD834};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t d_unit[] = {'D'};
	static const uint16_t low_unit[] = {0xDF06};
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
			SUITE, CASE_NAME, "failed to build range subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0, &result,
			&error) == 0,
			SUITE, CASE_NAME, "non-global range match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected non-global range match object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch range match[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, b_unit, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected first range match to be B");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"groups", 6, &value) == 0
			&& value.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "expected no groups object for range match");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, &result,
			&error) == 0,
			SUITE, CASE_NAME, "global range match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected three global range matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch global range match[1]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, d_unit, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected second global range match to be D");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 2, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch global range match[2]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_unit, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected third global range match to be lone low");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), &iterator,
			&error) == 0,
			SUITE, CASE_NAME, "range matchAll failed");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected first range matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch first range matchAll capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, b_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected first range matchAll capture to be B");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected second range matchAll result");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected third range matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch third range matchAll capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected third range matchAll capture to be lone low");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "expected exhausted range matchAll iterator");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_range_class(
			&region, high_subject, high_range,
			sizeof(high_range) / (2 * sizeof(high_range[0])), 0, &result,
			&error) == 0,
			SUITE, CASE_NAME, "isolated high range match failed");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch isolated high range capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_range, 1, &iterator)
			== 0 && jsval_strict_eq(&region, value, iterator) == 1,
			SUITE, CASE_NAME, "expected isolated high range capture");

	return generated_test_pass(SUITE, CASE_NAME);
}

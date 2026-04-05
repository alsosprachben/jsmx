#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-range-class-split-rewrite"

int
main(void)
{
	static const uint16_t range_subject_units[] = {
		'A', 0xD834, 0xDF06, 'B', 'D', 0xDF06, 'Z'
	};
	static const uint16_t b_to_d_and_low_ranges[] = {
		'B', 'D', 0xDF06, 0xDF06
	};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t z_unit[] = {'Z'};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t range_subject;
	jsval_t limit_two;
	jsval_t expected_value;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, range_subject_units,
			sizeof(range_subject_units) / sizeof(range_subject_units[0]),
			&range_subject) == 0,
			SUITE, CASE_NAME, "failed to build range subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"2", 1, &limit_two) == 0,
			SUITE, CASE_NAME, "failed to build split limit");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 0,
			jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "range split failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 4,
			SUITE, CASE_NAME, "expected four range split parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch range split[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_prefix_units,
			sizeof(pair_prefix_units) / sizeof(pair_prefix_units[0]),
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "unexpected range split prefix");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 3, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch range split[3]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, z_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "unexpected range split suffix");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_range_class(
			&region, range_subject, b_to_d_and_low_ranges,
			sizeof(b_to_d_and_low_ranges) /
			(2 * sizeof(b_to_d_and_low_ranges[0])), 1, limit_two,
			&result, &error) == 0,
			SUITE, CASE_NAME, "range split with limit failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two range split parts with limit");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch limited range split[1]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, NULL, 0,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected empty second limited range split part");

	return generated_test_pass(SUITE, CASE_NAME);
}

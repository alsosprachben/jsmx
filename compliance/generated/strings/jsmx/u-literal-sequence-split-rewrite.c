#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-sequence-split-rewrite"

static int
expect_utf16(jsval_region_t *region, jsval_t value, const uint16_t *expected,
		size_t expected_len)
{
	static const uint16_t empty_unit = 0;
	jsval_t expected_value;

	if (jsval_string_new_utf16(region,
			expected_len > 0 ? expected : &empty_unit, expected_len,
			&expected_value) < 0) {
		return -1;
	}
	return jsval_strict_eq(region, value, expected_value) == 1 ? 0 : -1;
}

int
main(void)
{
	static const uint16_t low_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'B', 'C'
	};
	static const uint16_t low_b_units[] = {0xDF06, 'B'};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t c_unit[] = {'C'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0,
			SUITE, CASE_NAME, "failed to build split subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"2", 1, &limit_two) == 0,
			SUITE, CASE_NAME, "failed to build split limit");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_sequence(
			&region, low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), 0,
			jsval_undefined(), &result, &error) == 0,
			SUITE, CASE_NAME, "sequence split failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected three split parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, pair_prefix_units,
				sizeof(pair_prefix_units) /
				sizeof(pair_prefix_units[0])) == 0,
			SUITE, CASE_NAME, "first split part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, NULL, 0) == 0,
			SUITE, CASE_NAME, "second split part should be empty");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 2, &value) == 0
			&& expect_utf16(&region, value, c_unit, 1) == 0,
			SUITE, CASE_NAME, "third split part mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_sequence(
			&region, low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), 1, limit_two,
			&result, &error) == 0,
			SUITE, CASE_NAME, "sequence split(limit) failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two split(limit) parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, pair_prefix_units,
				sizeof(pair_prefix_units) /
				sizeof(pair_prefix_units[0])) == 0,
			SUITE, CASE_NAME, "first split(limit) part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, NULL, 0) == 0,
			SUITE, CASE_NAME, "second split(limit) part should be empty");

	return generated_test_pass(SUITE, CASE_NAME);
}

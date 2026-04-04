#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-surrogate-split-rewrite"

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
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06, 'C'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'B', 0xD834, 'C'
	};
	static const uint16_t pair_prefix_units[] = {'A', 0xD834, 0xDF06};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t c_unit[] = {'C'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t high_subject;
	jsval_t limit_two;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored rewrite-backed fixture:
	 * lone-surrogate `/u` split helpers repeatedly scan for isolated
	 * surrogate code units while leaving surrogate pairs intact.
	 */
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0,
			SUITE, CASE_NAME, "failed to build low-surrogate split subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high-surrogate split subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"2", 1, &limit_two) == 0,
			SUITE, CASE_NAME, "failed to build split limit");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_surrogate(
			&region, low_subject, 0xDF06, 0, jsval_undefined(), &result,
			&error) == 0,
			SUITE, CASE_NAME, "low-surrogate split failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected three low-surrogate split parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, pair_prefix_units,
				sizeof(pair_prefix_units) /
				sizeof(pair_prefix_units[0])) == 0,
			SUITE, CASE_NAME, "first low-surrogate split part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, b_unit, 1) == 0,
			SUITE, CASE_NAME, "second low-surrogate split part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 2, &value) == 0
			&& expect_utf16(&region, value, c_unit, 1) == 0,
			SUITE, CASE_NAME, "third low-surrogate split part mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_surrogate(
			&region, low_subject, 0xDF06, 1, limit_two, &result,
			&error) == 0,
			SUITE, CASE_NAME, "low-surrogate split(limit) failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two low-surrogate split(limit) parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, pair_prefix_units,
				sizeof(pair_prefix_units) /
				sizeof(pair_prefix_units[0])) == 0,
			SUITE, CASE_NAME, "first low-surrogate split(limit) part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, b_unit, 1) == 0,
			SUITE, CASE_NAME, "second low-surrogate split(limit) part mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_literal_surrogate(
			&region, high_subject, 0xD834, 0, jsval_undefined(), &result,
			&error) == 0,
			SUITE, CASE_NAME, "high-surrogate split failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected three high-surrogate split parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, pair_prefix_units,
				sizeof(pair_prefix_units) /
				sizeof(pair_prefix_units[0])) == 0,
			SUITE, CASE_NAME, "first high-surrogate split part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, b_unit, 1) == 0,
			SUITE, CASE_NAME, "second high-surrogate split part mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 2, &value) == 0
			&& expect_utf16(&region, value, c_unit, 1) == 0,
			SUITE, CASE_NAME, "third high-surrogate split part mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-predefined-whitespace-class-split-rewrite"

int
main(void)
{
	static const uint16_t whitespace_subject_units[] = {
		'A', 0x00A0, 'B', 0x2028, 'C'
	};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t b_unit[] = {'B'};
	static const uint16_t c_unit[] = {'C'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t whitespace_subject;
	jsval_t limit_two;
	jsval_t expected_value;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region,
			whitespace_subject_units,
			sizeof(whitespace_subject_units) /
			sizeof(whitespace_subject_units[0]), &whitespace_subject) == 0,
			SUITE, CASE_NAME, "failed to build whitespace subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"2", 1, &limit_two) == 0,
			SUITE, CASE_NAME, "failed to build split limit");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_whitespace_class(
			&region, whitespace_subject, 0, jsval_undefined(), &result,
			&error) == 0,
			SUITE, CASE_NAME, "whitespace split failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected three whitespace split parts");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch whitespace split[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, a_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected whitespace split[0] to be A");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch whitespace split[1]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, b_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected whitespace split[1] to be B");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 2, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch whitespace split[2]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, c_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected whitespace split[2] to be C");

	GENERATED_TEST_ASSERT(jsval_method_string_split_u_whitespace_class(
			&region, whitespace_subject, 1, limit_two, &result,
			&error) == 0,
			SUITE, CASE_NAME, "whitespace split with limit failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two whitespace split parts with limit");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch limited whitespace split[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, a_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected limited whitespace split[0] to be A");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch limited whitespace split[1]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, b_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected limited whitespace split[1] to be B");

	return generated_test_pass(SUITE, CASE_NAME);
}

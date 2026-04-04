#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-surrogate-match-rewrite"

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
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 0xDF06
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'B', 0xD834
	};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t high_unit[] = {0xD834};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t high_subject;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored rewrite-backed fixture:
	 * String.prototype.match over a single literal lone-surrogate `/u` atom
	 * uses the surrogate-aware helper so it skips surrogate halves inside a
	 * pair while still returning isolated high/low matches.
	 */
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0,
			SUITE, CASE_NAME, "failed to build low-surrogate subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high-surrogate subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_surrogate(&region,
			low_subject, 0xDF06, 0, &result, &error) == 0,
			SUITE, CASE_NAME, "non-global surrogate match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected object result for non-global surrogate match");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to read match[0]");
	GENERATED_TEST_ASSERT(expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "non-global match should return isolated low surrogate");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"input", 5, &value) == 0,
			SUITE, CASE_NAME, "failed to read match.input");
	GENERATED_TEST_ASSERT(expect_utf16(&region, value, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0])) == 0,
			SUITE, CASE_NAME, "match.input should preserve the original string");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "non-global match index should be 3");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_surrogate(&region,
			low_subject, 0xDF06, 1, &result, &error) == 0,
			SUITE, CASE_NAME, "global low-surrogate match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two isolated low-surrogate matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "first global low-surrogate match mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "second global low-surrogate match mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_surrogate(&region,
			high_subject, 0xD834, 1, &result, &error) == 0,
			SUITE, CASE_NAME, "global high-surrogate match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two isolated high-surrogate matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, high_unit, 1) == 0,
			SUITE, CASE_NAME, "first global high-surrogate match mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, high_unit, 1) == 0,
			SUITE, CASE_NAME, "second global high-surrogate match mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

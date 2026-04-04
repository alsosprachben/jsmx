#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-sequence-match-rewrite"

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
	static const uint16_t pair_subject_units[] = {
		'A', 0xD834, 0xDF06, '!', 'X', 0xD834, 0xDF06, '!', 'Y'
	};
	static const uint16_t low_b_units[] = {0xDF06, 'B'};
	static const uint16_t pair_bang_units[] = {0xD834, 0xDF06, '!'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t low_subject;
	jsval_t pair_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0,
			SUITE, CASE_NAME, "failed to build low-sequence subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_subject_units,
			sizeof(pair_subject_units) / sizeof(pair_subject_units[0]),
			&pair_subject) == 0,
			SUITE, CASE_NAME, "failed to build pair-sequence subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_sequence(
			&region, pair_subject, pair_bang_units,
			sizeof(pair_bang_units) / sizeof(pair_bang_units[0]), 0, &result,
			&error) == 0,
			SUITE, CASE_NAME, "non-global pair-sequence match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected object result for non-global pair-sequence match");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, pair_bang_units,
				sizeof(pair_bang_units) /
				sizeof(pair_bang_units[0])) == 0,
			SUITE, CASE_NAME, "pair-sequence match[0] mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(1.0)) == 1,
			SUITE, CASE_NAME, "pair-sequence index should be 1");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"groups", 6, &value) == 0
			&& value.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "groups should remain undefined");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_sequence(
			&region, low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), 1, &result,
			&error) == 0,
			SUITE, CASE_NAME, "global low-sequence match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two low-sequence global matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, low_b_units,
				sizeof(low_b_units) / sizeof(low_b_units[0])) == 0,
			SUITE, CASE_NAME, "first low-sequence global match mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, low_b_units,
				sizeof(low_b_units) / sizeof(low_b_units[0])) == 0,
			SUITE, CASE_NAME, "second low-sequence global match mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_sequence(
			&region, low_subject, low_b_units,
			sizeof(low_b_units) / sizeof(low_b_units[0]), &iterator,
			&error) == 0,
			SUITE, CASE_NAME, "low-sequence matchAll helper failed");
	GENERATED_TEST_ASSERT(iterator.kind == JSVAL_KIND_MATCH_ITERATOR,
			SUITE, CASE_NAME, "expected sequence matchAll iterator");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done
			&& result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected first sequence matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, low_b_units,
				sizeof(low_b_units) / sizeof(low_b_units[0])) == 0,
			SUITE, CASE_NAME, "first sequence matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "first sequence matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done
			&& result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected second sequence matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, low_b_units,
				sizeof(low_b_units) / sizeof(low_b_units[0])) == 0,
			SUITE, CASE_NAME, "second sequence matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(5.0)) == 1,
			SUITE, CASE_NAME, "second sequence matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "sequence matchAll iterator should exhaust");

	return generated_test_pass(SUITE, CASE_NAME);
}

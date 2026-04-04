#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-class-match-rewrite"

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
	static const uint16_t class_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 'D', 0xD834, 'C'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t low_unit[] = {0xDF06};
	static const uint16_t d_unit[] = {'D'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t class_subject;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_subject) == 0,
			SUITE, CASE_NAME, "failed to build class subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 0, &result,
			&error) == 0,
			SUITE, CASE_NAME, "non-global class match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_OBJECT, SUITE, CASE_NAME,
			"expected object result for non-global class match");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "class match[0] mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "class match index should be 3");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"groups", 6, &value) == 0
			&& value.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "groups should remain undefined");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_literal_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), 1, &result,
			&error) == 0,
			SUITE, CASE_NAME, "global class match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 2,
			SUITE, CASE_NAME, "expected two global class matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "first global class match mismatch");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0
			&& expect_utf16(&region, value, d_unit, 1) == 0,
			SUITE, CASE_NAME, "second global class match mismatch");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_class(
			&region, class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &iterator,
			&error) == 0,
			SUITE, CASE_NAME, "class matchAll helper failed");
	GENERATED_TEST_ASSERT(iterator.kind == JSVAL_KIND_MATCH_ITERATOR,
			SUITE, CASE_NAME, "expected class matchAll iterator");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done
			&& result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected first class matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "first class matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "first class matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done
			&& result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected second class matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, d_unit, 1) == 0,
			SUITE, CASE_NAME, "second class matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(5.0)) == 1,
			SUITE, CASE_NAME, "second class matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "class matchAll iterator should exhaust");

	return generated_test_pass(SUITE, CASE_NAME);
}

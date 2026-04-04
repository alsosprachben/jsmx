#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-surrogate-matchAll-rewrite"

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
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored rewrite-backed fixture:
	 * lone-surrogate `/u` matchAll lowers to a dedicated iterator helper that
	 * yields exec-shaped match objects for isolated surrogates only.
	 */
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, low_subject_units,
			sizeof(low_subject_units) / sizeof(low_subject_units[0]),
			&low_subject) == 0,
			SUITE, CASE_NAME, "failed to build low-surrogate subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high-surrogate subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_surrogate(
			&region, low_subject, 0xDF06, &iterator, &error) == 0,
			SUITE, CASE_NAME, "low-surrogate matchAll helper failed");
	GENERATED_TEST_ASSERT(iterator.kind == JSVAL_KIND_MATCH_ITERATOR,
			SUITE, CASE_NAME, "expected low-surrogate matchAll iterator");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done && result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected first low-surrogate iterator result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "first low-surrogate matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "first low-surrogate matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"input", 5, &value) == 0
			&& expect_utf16(&region, value, low_subject_units,
				sizeof(low_subject_units) /
				sizeof(low_subject_units[0])) == 0,
			SUITE, CASE_NAME, "matchAll input should preserve the original string");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"groups", 6, &value) == 0
			&& value.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "groups should remain undefined");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done && result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected second low-surrogate iterator result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, low_unit, 1) == 0,
			SUITE, CASE_NAME, "second low-surrogate matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(5.0)) == 1,
			SUITE, CASE_NAME, "second low-surrogate matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "low-surrogate iterator should be exhausted");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_surrogate(
			&region, high_subject, 0xD834, &iterator, &error) == 0,
			SUITE, CASE_NAME, "high-surrogate matchAll helper failed");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done && result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected first high-surrogate iterator result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, high_unit, 1) == 0,
			SUITE, CASE_NAME, "first high-surrogate matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "first high-surrogate matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done && result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected second high-surrogate iterator result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0
			&& expect_utf16(&region, value, high_unit, 1) == 0,
			SUITE, CASE_NAME, "second high-surrogate matchAll capture mismatch");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"index", 5, &value) == 0
			&& jsval_strict_eq(&region, value, jsval_number(5.0)) == 1,
			SUITE, CASE_NAME, "second high-surrogate matchAll index mismatch");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "high-surrogate iterator should be exhausted");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-predefined-word-class-match-rewrite"

int
main(void)
{
	static const uint16_t word_subject_units[] = {
		0xD834, 0xDF06, '!', 'A', '1', '_', 0xD834, '?'
	};
	static const uint16_t bang_unit[] = {'!'};
	static const uint16_t a_unit[] = {'A'};
	static const uint16_t one_unit[] = {'1'};
	static const uint16_t underscore_unit[] = {'_'};
	static const uint16_t high_unit[] = {0xD834};
	static const uint16_t q_unit[] = {'?'};
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t word_subject;
	jsval_t expected_value;
	jsval_t iterator;
	jsval_t result;
	jsval_t value;
	jsmethod_error_t error;
	int done;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, word_subject_units,
			sizeof(word_subject_units) / sizeof(word_subject_units[0]),
			&word_subject) == 0,
			SUITE, CASE_NAME, "failed to build word subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_word_class(&region,
			word_subject, 1, &result, &error) == 0,
			SUITE, CASE_NAME, "global word match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY
			&& jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected three global \\w matches");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 0, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch global word match[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, a_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected first global \\w match to be A");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch global word match[1]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, one_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected second global \\w match to be 1");
	GENERATED_TEST_ASSERT(jsval_array_get(&region, result, 2, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch global word match[2]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, underscore_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected third global \\w match to be _");

	GENERATED_TEST_ASSERT(jsval_method_string_match_u_negated_word_class(
			&region, word_subject, 0, &result, &error) == 0,
			SUITE, CASE_NAME, "non-global negated word match failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_OBJECT,
			SUITE, CASE_NAME, "expected non-global \\W match object");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch negated word match[0]");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, bang_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected first \\W match to be !");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"groups", 6, &value) == 0
			&& value.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "expected no groups object for \\W match");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_negated_word_class(
			&region, word_subject, &iterator, &error) == 0,
			SUITE, CASE_NAME, "negated word matchAll failed");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected first \\W matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch first \\W matchAll capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, bang_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected first \\W matchAll capture to be !");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected second \\W matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch second \\W matchAll capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected second \\W matchAll capture to be isolated high");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && !done,
			SUITE, CASE_NAME, "expected third \\W matchAll result");
	GENERATED_TEST_ASSERT(jsval_object_get_utf8(&region, result,
			(const uint8_t *)"0", 1, &value) == 0,
			SUITE, CASE_NAME, "failed to fetch third \\W matchAll capture");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, q_unit, 1,
			&expected_value) == 0
			&& jsval_strict_eq(&region, value, expected_value) == 1,
			SUITE, CASE_NAME, "expected third \\W matchAll capture to be ?");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "expected exhausted \\W matchAll iterator");

	return generated_test_pass(SUITE, CASE_NAME);
}

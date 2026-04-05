#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-predefined-digit-class-search-rewrite"

int
main(void)
{
	static const uint16_t digit_subject_units[] = {
		0xD834, 0xDF06, 'A', '1', '2', 'B'
	};
	static const uint16_t pair_only_units[] = {0xD834, 0xDF06};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t digit_subject;
	jsval_t pair_only;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, digit_subject_units,
			sizeof(digit_subject_units) /
			sizeof(digit_subject_units[0]), &digit_subject) == 0,
			SUITE, CASE_NAME, "failed to build digit subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0,
			SUITE, CASE_NAME, "failed to build pair-only subject");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_digit_class(&region,
			digit_subject, &result, &error) == 0,
			SUITE, CASE_NAME, "digit search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "expected \\d search index 3");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_negated_digit_class(
			&region, digit_subject, &result, &error) == 0,
			SUITE, CASE_NAME, "negated digit search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(2.0)) == 1,
			SUITE, CASE_NAME, "expected \\D search index 2");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_digit_class(&region,
			pair_only, &result, &error) == 0,
			SUITE, CASE_NAME, "digit search on pair-only subject failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(-1.0)) == 1,
			SUITE, CASE_NAME, "expected no \\d match inside surrogate pair");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_negated_digit_class(
			&region, pair_only, &result, &error) == 0,
			SUITE, CASE_NAME, "negated digit search on pair-only subject failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(-1.0)) == 1,
			SUITE, CASE_NAME, "expected no \\D match inside surrogate pair");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-class-search-rewrite"

int
main(void)
{
	static const uint16_t class_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xDF06, 'B', 'D', 0xD834, 'C'
	};
	static const uint16_t pair_only_units[] = {
		'A', 0xD834, 0xDF06, '!'
	};
	static const uint16_t high_subject_units[] = {
		'A', 0xD834, 0xDF06, 0xD834, 'C'
	};
	static const uint16_t low_d_members[] = {0xDF06, 'D'};
	static const uint16_t high_members[] = {0xD834};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t class_subject;
	jsval_t pair_only;
	jsval_t high_subject;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, class_subject_units,
			sizeof(class_subject_units) / sizeof(class_subject_units[0]),
			&class_subject) == 0,
			SUITE, CASE_NAME, "failed to build class subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0,
			SUITE, CASE_NAME, "failed to build pair-only subject");
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, high_subject_units,
			sizeof(high_subject_units) / sizeof(high_subject_units[0]),
			&high_subject) == 0,
			SUITE, CASE_NAME, "failed to build high-subject");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_class(&region,
			class_subject, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &result,
			&error) == 0,
			SUITE, CASE_NAME, "class search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "expected first class search hit at 3");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_class(&region,
			pair_only, low_d_members,
			sizeof(low_d_members) / sizeof(low_d_members[0]), &result,
			&error) == 0,
			SUITE, CASE_NAME, "pair-only class search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(-1.0)) == 1,
			SUITE, CASE_NAME, "class search should not match inside a pair");

	GENERATED_TEST_ASSERT(jsval_method_string_search_u_literal_class(&region,
			high_subject, high_members,
			sizeof(high_members) / sizeof(high_members[0]), &result,
			&error) == 0,
			SUITE, CASE_NAME, "isolated high-surrogate class search failed");
	GENERATED_TEST_ASSERT(jsval_strict_eq(&region, result,
			jsval_number(3.0)) == 1,
			SUITE, CASE_NAME, "expected isolated high-surrogate hit at 3");

	return generated_test_pass(SUITE, CASE_NAME);
}

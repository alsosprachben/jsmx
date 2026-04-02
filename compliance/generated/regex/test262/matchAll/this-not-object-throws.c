#include "compliance/generated/regex_match_all_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/matchAll/this-not-object-throws"

int
main(void)
{
	uint8_t storage[8192];
	jsval_region_t region;
	jsval_t subject;
	jsval_t iterator;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"", 0, &subject) == 0,
			SUITE, CASE_NAME, "subject build failed");

	GENERATED_TEST_ASSERT(jsval_regexp_match_all(&region, jsval_null(), subject,
			&iterator, &error) == -1,
			SUITE, CASE_NAME, "expected null receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for null receiver, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsval_regexp_match_all(&region, jsval_bool(1), subject,
			&iterator, &error) == -1,
			SUITE, CASE_NAME, "expected boolean receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for boolean receiver, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsval_regexp_match_all(&region, jsval_number(1.0),
			subject, &iterator, &error) == -1,
			SUITE, CASE_NAME, "expected number receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for number receiver, got %d",
			(int)error.kind);

	GENERATED_TEST_ASSERT(jsval_regexp_match_all(&region, subject, subject,
			&iterator, &error) == -1,
			SUITE, CASE_NAME, "expected string receiver failure");
	GENERATED_TEST_ASSERT(error.kind == JSMETHOD_ERROR_TYPE, SUITE,
			CASE_NAME, "expected TYPE for string receiver, got %d",
			(int)error.kind);
	return generated_test_pass(SUITE, CASE_NAME);
}

#include "compliance/generated/regex_match_all_helpers.h"

#define SUITE "regex"
#define CASE_NAME "test262/matchAll/this-lastindex-cached"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t regexp;
	jsval_t input_value;
	jsval_t iterator;
	jsval_t result;
	jsmethod_error_t error;
	size_t last_index = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(generated_regexp_new_utf8(&region, ".", "g",
			&regexp, &error) == 0,
			SUITE, CASE_NAME, "regex build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, regexp, 2) == 0,
			SUITE, CASE_NAME, "lastIndex set failed");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"abcd", 4, &input_value) == 0,
			SUITE, CASE_NAME, "input build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_match_all(&region, regexp, input_value,
			&iterator, &error) == 0,
			SUITE, CASE_NAME, "matchAll iterator build failed");
	GENERATED_TEST_ASSERT(jsval_regexp_set_last_index(&region, regexp, 0) == 0,
			SUITE, CASE_NAME, "lastIndex reset failed");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			iterator, &result, SUITE, CASE_NAME, "first match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first iterator step mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"0", "c", SUITE, CASE_NAME, "first match[0]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 2.0, SUITE, CASE_NAME, "first index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			iterator, &result, SUITE, CASE_NAME, "second match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second iterator step mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"0", "d", SUITE, CASE_NAME, "second match[0]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 3.0, SUITE, CASE_NAME, "second index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_done(&region,
			iterator, SUITE, CASE_NAME, "done")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "expected iterator completion");
	GENERATED_TEST_ASSERT(jsval_regexp_get_last_index(&region, regexp,
			&last_index) == 0,
			SUITE, CASE_NAME, "lastIndex read failed");
	GENERATED_TEST_ASSERT(last_index == 0, SUITE, CASE_NAME,
			"expected original regex lastIndex to remain reset, got %zu",
			last_index);
	return generated_test_pass(SUITE, CASE_NAME);
}

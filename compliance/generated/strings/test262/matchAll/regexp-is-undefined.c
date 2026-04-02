#include "compliance/generated/regex_match_all_helpers.h"

#define SUITE "strings"
#define CASE_NAME "test262/matchAll/regexp-is-undefined"

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t text;
	jsval_t iterator;
	jsval_t result;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"a", 1, &text) == 0,
			SUITE, CASE_NAME, "text build failed");
	GENERATED_TEST_ASSERT(jsval_method_string_match_all(&region, text, 1,
			jsval_undefined(), &iterator, &error) == 0,
			SUITE, CASE_NAME, "matchAll(undefined) failed");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			iterator, &result, SUITE, CASE_NAME, "first match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first iterator step mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"0", "", SUITE, CASE_NAME, "first match[0]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 0.0, SUITE, CASE_NAME, "first index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "first index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_next(&region,
			iterator, &result, SUITE, CASE_NAME, "second match")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second iterator step mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_string_prop(&region, result,
			"0", "", SUITE, CASE_NAME, "second match[0]")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second match[0] mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_number_prop(&region, result,
			"index", 1.0, SUITE, CASE_NAME, "second index")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "second index mismatch");
	GENERATED_TEST_ASSERT(generated_expect_match_iterator_done(&region,
			iterator, SUITE, CASE_NAME, "done")
			== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "expected iterator completion");
	return generated_test_pass(SUITE, CASE_NAME);
}

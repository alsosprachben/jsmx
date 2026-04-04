#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-surrogate-matchAll-pair-skip-rewrite"

int
main(void)
{
	static const uint16_t pair_only_units[] = {0xD834, 0xDF06};
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t pair_only;
	jsval_t iterator;
	jsval_t result;
	jsmethod_error_t error;
	int done = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	/*
	 * Repo-authored rewrite-backed fixture:
	 * the lone-surrogate `/u` matchAll helper must skip surrogate halves inside
	 * a well-formed pair, so both the high- and low-surrogate rewrites
	 * produce an already-exhausted iterator for a pair-only subject.
	 */
	GENERATED_TEST_ASSERT(jsval_string_new_utf16(&region, pair_only_units,
			sizeof(pair_only_units) / sizeof(pair_only_units[0]),
			&pair_only) == 0,
			SUITE, CASE_NAME, "failed to build pair-only subject");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_surrogate(
			&region, pair_only, 0xDF06, &iterator, &error) == 0,
			SUITE, CASE_NAME, "low-surrogate matchAll helper failed");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "low-surrogate iterator should skip pair halves");

	GENERATED_TEST_ASSERT(jsval_method_string_match_all_u_literal_surrogate(
			&region, pair_only, 0xD834, &iterator, &error) == 0,
			SUITE, CASE_NAME, "high-surrogate matchAll helper failed");
	GENERATED_TEST_ASSERT(jsval_match_iterator_next(&region, iterator, &done,
			&result, &error) == 0 && done
			&& result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "high-surrogate iterator should skip pair halves");

	return generated_test_pass(SUITE, CASE_NAME);
}

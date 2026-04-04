#include "compliance/generated/test_contract.h"
#include "jsregex.h"

#define SUITE "strings"
#define CASE_NAME "jsmx/u-literal-surrogate-search-rewrite"

int
main(void)
{
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	static const uint16_t pair_then_low[] = {0xD834, 0xDF06, 0xDF06};
	static const uint16_t pair_then_high[] = {0xD834, 0xDF06, 0xD834};
	jsregex_search_result_t result;

	/*
	 * Repo-authored rewrite-backed fixture:
	 * search semantics over a single literal lone-surrogate `/u` atom use the
	 * same surrogate-aware UTF-16 scan as exec/test and report the first
	 * isolated occurrence index, not a surrogate half from inside a pair.
	 */
	GENERATED_TEST_ASSERT(jsregex_search_u_literal_surrogate_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), 0xDF06, 0,
			&result) == 0,
			SUITE, CASE_NAME, "low-surrogate search failed");
	GENERATED_TEST_ASSERT(result.matched == 0, SUITE, CASE_NAME,
			"low-surrogate search must miss inside a pair");

	GENERATED_TEST_ASSERT(jsregex_search_u_literal_surrogate_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), 0xD834, 0,
			&result) == 0,
			SUITE, CASE_NAME, "high-surrogate search failed");
	GENERATED_TEST_ASSERT(result.matched == 0, SUITE, CASE_NAME,
			"high-surrogate search must miss inside a pair");

	GENERATED_TEST_ASSERT(jsregex_search_u_literal_surrogate_utf16(pair_then_low,
			sizeof(pair_then_low) / sizeof(pair_then_low[0]), 0xDF06, 0,
			&result) == 0,
			SUITE, CASE_NAME, "isolated low-surrogate search failed");
	GENERATED_TEST_ASSERT(result.matched == 1, SUITE, CASE_NAME,
			"isolated low-surrogate search should match");
	GENERATED_TEST_ASSERT(result.start == 2 && result.end == 3,
			SUITE, CASE_NAME, "isolated low-surrogate search span mismatch");

	GENERATED_TEST_ASSERT(jsregex_search_u_literal_surrogate_utf16(pair_then_high,
			sizeof(pair_then_high) / sizeof(pair_then_high[0]), 0xD834, 0,
			&result) == 0,
			SUITE, CASE_NAME, "isolated high-surrogate search failed");
	GENERATED_TEST_ASSERT(result.matched == 1, SUITE, CASE_NAME,
			"isolated high-surrogate search should match");
	GENERATED_TEST_ASSERT(result.start == 2 && result.end == 3,
			SUITE, CASE_NAME, "isolated high-surrogate search span mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

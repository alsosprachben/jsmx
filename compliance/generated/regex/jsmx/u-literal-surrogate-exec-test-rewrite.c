#include "compliance/generated/test_contract.h"
#include "jsregex.h"

#define SUITE "regex"
#define CASE_NAME "jsmx/u-literal-surrogate-exec-test-rewrite"

int
main(void)
{
	static const uint16_t pair_only[] = {0xD834, 0xDF06};
	static const uint16_t pair_then_low[] = {0xD834, 0xDF06, 0xDF06};
	static const uint16_t pair_then_high[] = {0xD834, 0xDF06, 0xD834};
	jsregex_exec_result_t exec_result;
	int matched = 0;

	/*
	 * Repo-authored rewrite-backed fixture:
	 * single literal lone-surrogate atoms under `/u` use explicit UTF-16 scan
	 * helpers so they do not match surrogate halves inside a pair, but still
	 * match isolated surrogate code units.
	 */
	GENERATED_TEST_ASSERT(jsregex_exec_u_literal_surrogate_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), 0xDF06, 0,
			&exec_result) == 0,
			SUITE, CASE_NAME, "low-surrogate exec failed");
	GENERATED_TEST_ASSERT(exec_result.matched == 0, SUITE, CASE_NAME,
			"low surrogate must not match trailing half of a pair");

	GENERATED_TEST_ASSERT(jsregex_exec_u_literal_surrogate_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), 0xD834, 0,
			&exec_result) == 0,
			SUITE, CASE_NAME, "high-surrogate exec failed");
	GENERATED_TEST_ASSERT(exec_result.matched == 0, SUITE, CASE_NAME,
			"high surrogate must not match leading half of a pair");

	GENERATED_TEST_ASSERT(jsregex_exec_u_literal_surrogate_utf16(pair_then_low,
			sizeof(pair_then_low) / sizeof(pair_then_low[0]), 0xDF06, 0,
			&exec_result) == 0,
			SUITE, CASE_NAME, "isolated low-surrogate exec failed");
	GENERATED_TEST_ASSERT(exec_result.matched == 1, SUITE, CASE_NAME,
			"isolated low surrogate should match");
	GENERATED_TEST_ASSERT(exec_result.start == 2 && exec_result.end == 3,
			SUITE, CASE_NAME, "isolated low surrogate match span mismatch");

	GENERATED_TEST_ASSERT(jsregex_exec_u_literal_surrogate_utf16(pair_then_high,
			sizeof(pair_then_high) / sizeof(pair_then_high[0]), 0xD834, 0,
			&exec_result) == 0,
			SUITE, CASE_NAME, "isolated high-surrogate exec failed");
	GENERATED_TEST_ASSERT(exec_result.matched == 1, SUITE, CASE_NAME,
			"isolated high surrogate should match");
	GENERATED_TEST_ASSERT(exec_result.start == 2 && exec_result.end == 3,
			SUITE, CASE_NAME, "isolated high surrogate match span mismatch");

	GENERATED_TEST_ASSERT(jsregex_test_u_literal_surrogate_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), 0xDF06, 0,
			&matched) == 0,
			SUITE, CASE_NAME, "low-surrogate test failed");
	GENERATED_TEST_ASSERT(matched == 0, SUITE, CASE_NAME,
			"low-surrogate test should be false inside a pair");

	GENERATED_TEST_ASSERT(jsregex_test_u_literal_surrogate_utf16(pair_then_low,
			sizeof(pair_then_low) / sizeof(pair_then_low[0]), 0xDF06, 0,
			&matched) == 0,
			SUITE, CASE_NAME, "isolated low-surrogate test failed");
	GENERATED_TEST_ASSERT(matched == 1, SUITE, CASE_NAME,
			"isolated low-surrogate test should be true");

	GENERATED_TEST_ASSERT(jsregex_test_u_literal_surrogate_utf16(pair_only,
			sizeof(pair_only) / sizeof(pair_only[0]), 0xD834, 0,
			&matched) == 0,
			SUITE, CASE_NAME, "high-surrogate test failed");
	GENERATED_TEST_ASSERT(matched == 0, SUITE, CASE_NAME,
			"high-surrogate test should be false inside a pair");

	GENERATED_TEST_ASSERT(jsregex_test_u_literal_surrogate_utf16(pair_then_high,
			sizeof(pair_then_high) / sizeof(pair_then_high[0]), 0xD834, 0,
			&matched) == 0,
			SUITE, CASE_NAME, "isolated high-surrogate test failed");
	GENERATED_TEST_ASSERT(matched == 1, SUITE, CASE_NAME,
			"isolated high-surrogate test should be true");

	return generated_test_pass(SUITE, CASE_NAME);
}

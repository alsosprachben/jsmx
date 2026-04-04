#include "compliance/generated/test_contract.h"
#include "jsregex.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/u-lastindex-adv"

int
main(void)
{
	static const uint16_t subject[] = {0xD834, 0xDF06};
	jsregex_exec_result_t result;

	/*
	 * Upstream source: test262 string/exec u-lastindex-adv.
	 *
	 * Rewrite-backed translation: `/\udf06/u` is a single literal low-surrogate
	 * atom under the Unicode flag. PCRE2 UTF mode cannot compile that atom
	 * honestly, so the translator lowers it to an explicit UTF-16 scan that
	 * matches only isolated occurrences of the surrogate code unit, not the
	 * trailing half of a surrogate pair.
	 */
	GENERATED_TEST_ASSERT(jsregex_exec_u_literal_surrogate_utf16(subject,
			sizeof(subject) / sizeof(subject[0]), 0xDF06, 0, &result) == 0,
			SUITE, CASE_NAME, "rewrite-backed surrogate exec failed");
	GENERATED_TEST_ASSERT(result.matched == 0, SUITE, CASE_NAME,
			"expected null result for low-surrogate /u atom inside a pair");
	return generated_test_pass(SUITE, CASE_NAME);
}

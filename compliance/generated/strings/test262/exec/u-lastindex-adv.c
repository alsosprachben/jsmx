#include "compliance/generated/test_contract.h"

#define SUITE "strings"
#define CASE_NAME "test262/exec/u-lastindex-adv"

int
main(void)
{
	/*
	 * Upstream source: test262 string/exec u-lastindex-adv.
	 *
	 * This is a JS-valid unicode/surrogate-sensitive /u exec case. The current
	 * PCRE2-backed runtime does not model it honestly as a direct-lowered pass,
	 * and no reviewed rewrite-backed lowering is committed yet.
	 */
	return generated_test_known_unsupported(SUITE, CASE_NAME,
			"JS-valid unicode/surrogate-sensitive /u exec case is not "
			"yet an honest direct or rewrite-backed lowering");
}

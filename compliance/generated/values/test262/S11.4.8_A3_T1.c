#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.4.8_A3_T1"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/bitwise-not/S11.4.8_A3_T1.js
	 *
	 * This idiomatic flattened translation preserves the primitive boolean
	 * check. The Boolean-object checks in CHECK#2 and CHECK#3 are intentionally
	 * omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_bitwise_not(&region, jsval_bool(0), &result)
			== 0, SUITE, CASE_NAME, "failed to lower ~false");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, -1.0, SUITE,
			CASE_NAME, "~false") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for ~false");

	return generated_test_pass(SUITE, CASE_NAME);
}

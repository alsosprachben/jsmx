#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.1_A6.1"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;

	/*
	 * Generated from test262:
	 * test/language/expressions/equals/S11.9.1_A6.1.js
	 *
	 * This idiomatic flattened translation preserves the primitive
	 * undefined/null checks. `void 0` and `eval("var x")` both collapse to the
	 * undefined value in this lowered form.
	 */

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), jsval_undefined(), 1, SUITE, CASE_NAME,
			"undefined == undefined") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for undefined == undefined");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), jsval_undefined(), 1, SUITE, CASE_NAME,
			"void 0 == undefined") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for void 0 == undefined");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), jsval_undefined(), 1, SUITE, CASE_NAME,
			"undefined == eval(\"var x\")") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for undefined == eval(\"var x\")");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), jsval_null(), 1, SUITE, CASE_NAME,
			"undefined == null") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for undefined == null");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_null(), jsval_undefined(), 1, SUITE, CASE_NAME,
			"null == void 0") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for null == void 0");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_null(), jsval_null(), 1, SUITE, CASE_NAME,
			"null == null") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for null == null");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.1_A6.2_T1"

int
main(void)
{
	uint8_t storage[1024];
	jsval_region_t region;
	jsval_t undefined_string;
	jsval_t null_string;

	/*
	 * Generated from test262:
	 * test/language/expressions/equals/S11.9.1_A6.2_T1.js
	 *
	 * This idiomatic flattened translation preserves the primitive
	 * undefined/null vs non-nullish checks. The object comparisons in CHECK#4
	 * and CHECK#8 are intentionally omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"undefined", 9, &undefined_string) == 0, SUITE,
			CASE_NAME, "failed to construct string operand \"undefined\"");
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"null", 4, &null_string) == 0, SUITE, CASE_NAME,
			"failed to construct string operand \"null\"");

	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), jsval_bool(1), 0, SUITE, CASE_NAME,
			"undefined == true") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for undefined == true");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), jsval_number(0.0), 0, SUITE, CASE_NAME,
			"undefined == 0") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for undefined == 0");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_undefined(), undefined_string, 0, SUITE, CASE_NAME,
			"undefined == \"undefined\"") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for undefined == \"undefined\"");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region,
			jsval_null(), jsval_bool(0), 0, SUITE, CASE_NAME,
			"null == false") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for null == false");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, jsval_null(),
			jsval_number(0.0), 0, SUITE, CASE_NAME, "null == 0")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for null == 0");
	GENERATED_TEST_ASSERT(generated_expect_abstract_eq(&region, jsval_null(),
			null_string, 0, SUITE, CASE_NAME, "null == \"null\"")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for null == \"null\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

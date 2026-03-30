#include <math.h>
#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.9.2_A4.1_T1"

int
main(void)
{
	uint8_t storage[1024];
	jsval_region_t region;
	jsval_t string_value;

	/*
	 * Generated from test262:
	 * test/language/expressions/does-not-equals/S11.9.2_A4.1_T1.js
	 *
	 * This idiomatic flattened translation preserves the primitive NaN checks.
	 * The object comparison in CHECK#9 is intentionally omitted.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_string_new_utf8(&region,
			(const uint8_t *)"string", 6, &string_value) == 0, SUITE,
			CASE_NAME, "failed to construct string operand");

	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_bool(1), 1, SUITE, CASE_NAME,
			"NaN != true") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != true");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_number(1.0), 1, SUITE, CASE_NAME,
			"NaN != 1") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != 1");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_number(NAN), 1, SUITE, CASE_NAME,
			"NaN != NaN") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != NaN");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_number(INFINITY), 1, SUITE, CASE_NAME,
			"NaN != +Infinity") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != +Infinity");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_number(-INFINITY), 1, SUITE, CASE_NAME,
			"NaN != -Infinity") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != -Infinity");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_number(1.7976931348623157e+308), 1, SUITE,
			CASE_NAME, "NaN != Number.MAX_VALUE")
			== GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != Number.MAX_VALUE");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), jsval_number(5e-324), 1, SUITE, CASE_NAME,
			"NaN != Number.MIN_VALUE") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for NaN != Number.MIN_VALUE");
	GENERATED_TEST_ASSERT(generated_expect_abstract_ne(&region,
			jsval_number(NAN), string_value, 1, SUITE, CASE_NAME,
			"NaN != \"string\"") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for NaN != \"string\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.7.2_A3_T2.1"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/right-shift/S11.7.2_A3_T2.1.js
	 *
	 * This is an idiomatic slow-path translation. Primitive operands lower
	 * directly through jsval_shift_right, and the boxed Boolean/Number cases are
	 * preserved through explicit translator-emitted boxed-wrapper coercion
	 * helpers outside jsmx.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, jsval_bool(1),
			jsval_number(1.0), &result) == 0, SUITE, CASE_NAME,
			"failed to lower true >> 1");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "true >> 1") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for true >> 1");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, jsval_number(1.0),
			jsval_bool(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower 1 >> true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "1 >> true") == GENERATED_TEST_PASS, SUITE, CASE_NAME,
			"unexpected result for 1 >> true");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, generated_slow_boxed_boolean(1),
			jsval_number(1.0), &result) == 0, SUITE, CASE_NAME,
			"failed to lower boxed true >> 1");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "boxed true >> 1") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for boxed true >> 1");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, jsval_number(1.0),
			generated_slow_boxed_boolean(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower 1 >> boxed true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "1 >> boxed true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for 1 >> boxed true");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, jsval_bool(1),
			generated_slow_boxed_number(1.0), &result) == 0, SUITE, CASE_NAME,
			"failed to lower true >> boxed 1");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "true >> boxed 1") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for true >> boxed 1");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, generated_slow_boxed_number(1.0),
			jsval_bool(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower boxed 1 >> true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "boxed 1 >> true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for boxed 1 >> true");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, generated_slow_boxed_boolean(1),
			generated_slow_boxed_number(1.0), &result) == 0, SUITE, CASE_NAME,
			"failed to lower boxed true >> boxed 1");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "boxed true >> boxed 1") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for boxed true >> boxed 1");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, generated_slow_boxed_number(1.0),
			generated_slow_boxed_boolean(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower boxed 1 >> boxed true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "boxed 1 >> boxed true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for boxed 1 >> boxed true");

	return generated_test_pass(SUITE, CASE_NAME);
}

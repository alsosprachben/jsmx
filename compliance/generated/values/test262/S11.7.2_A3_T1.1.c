#include <stdint.h>

#include "compliance/generated/value_helpers.h"

#define SUITE "values"
#define CASE_NAME "test262/S11.7.2_A3_T1.1"

int
main(void)
{
	uint8_t storage[256];
	jsval_region_t region;
	jsval_t result;

	/*
	 * Generated from test262:
	 * test/language/expressions/right-shift/S11.7.2_A3_T1.1.js
	 *
	 * This is an idiomatic slow-path translation. Primitive operands lower
	 * directly through jsval_shift_right, and the boxed Boolean cases are
	 * preserved through an explicit translator-emitted boxed-wrapper coercion
	 * helper outside jsmx.
	 */

	jsval_region_init(&region, storage, sizeof(storage));
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, jsval_bool(1), jsval_bool(1),
			&result) == 0, SUITE, CASE_NAME, "failed to lower true >> true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "true >> true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for true >> true");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, generated_slow_boxed_boolean(1),
			jsval_bool(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower boxed true >> true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "boxed true >> true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for boxed true >> true");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, jsval_bool(1),
			generated_slow_boxed_boolean(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower true >> boxed true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "true >> boxed true") == GENERATED_TEST_PASS, SUITE,
			CASE_NAME, "unexpected result for true >> boxed true");
	GENERATED_TEST_ASSERT(jsval_shift_right(&region, generated_slow_boxed_boolean(1),
			generated_slow_boxed_boolean(1), &result) == 0, SUITE, CASE_NAME,
			"failed to lower boxed true >> boxed true");
	GENERATED_TEST_ASSERT(generated_expect_number(&region, result, 0.0, SUITE,
			CASE_NAME, "boxed true >> boxed true") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"unexpected result for boxed true >> boxed true");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/all-settled-mix"

static int
expect_number_at(jsval_region_t *region, jsval_t array, size_t index,
		double expected)
{
	jsval_t elem;
	if (jsval_array_get(region, array, index, &elem) != 0 ||
			elem.kind != JSVAL_KIND_NUMBER ||
			elem.as.number != expected) {
		return generated_test_fail(SUITE, CASE_NAME,
				"expected results[%zu] == %g", index, expected);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t inputs[3];
	jsval_t all;
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	/* inputs[0] stays pending; inputs[1] and [2] pre-fulfilled. */
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[0]) == 0,
			SUITE, CASE_NAME, "failed to create inputs[0]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[1]) == 0 &&
			jsval_promise_resolve(&region, inputs[1],
					jsval_number(200.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[1]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[2]) == 0 &&
			jsval_promise_resolve(&region, inputs[2],
					jsval_number(300.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[2]");

	GENERATED_TEST_ASSERT(
			jsval_promise_all(&region, inputs, 3, &all) == 0,
			SUITE, CASE_NAME, "jsval_promise_all failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME, "expected PENDING before last resolve");

	/* Resolve the last pending input; drain; the coord scan inside
	 * jsval_microtask_drain should fulfill the aggregate. */
	GENERATED_TEST_ASSERT(
			jsval_promise_resolve(&region, inputs[0],
					jsval_number(100.0)) == 0,
			SUITE, CASE_NAME, "failed to resolve inputs[0]");
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(
			jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "microtask drain failed");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "expected FULFILLED after drain");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, all, &result) == 0 &&
			result.kind == JSVAL_KIND_ARRAY &&
			jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected 3-element array result");
	GENERATED_TEST_ASSERT(
			expect_number_at(&region, result, 0, 100.0) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[0] mismatch");
	GENERATED_TEST_ASSERT(
			expect_number_at(&region, result, 1, 200.0) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[1] mismatch");
	GENERATED_TEST_ASSERT(
			expect_number_at(&region, result, 2, 300.0) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[2] mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

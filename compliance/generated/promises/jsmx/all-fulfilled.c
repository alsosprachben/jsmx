#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/all-fulfilled"

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
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t inputs[3];
	jsval_t all;
	jsval_t result;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[0]) == 0 &&
			jsval_promise_resolve(&region, inputs[0],
					jsval_number(10.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[0]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[1]) == 0 &&
			jsval_promise_resolve(&region, inputs[1],
					jsval_number(20.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[1]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[2]) == 0 &&
			jsval_promise_resolve(&region, inputs[2],
					jsval_number(30.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[2]");

	GENERATED_TEST_ASSERT(
			jsval_promise_all(&region, inputs, 3, &all) == 0,
			SUITE, CASE_NAME, "jsval_promise_all failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "expected FULFILLED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, all, &result) == 0 &&
			result.kind == JSVAL_KIND_ARRAY,
			SUITE, CASE_NAME, "expected array result");
	GENERATED_TEST_ASSERT(
			jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected length 3");
	GENERATED_TEST_ASSERT(
			expect_number_at(&region, result, 0, 10.0) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[0] mismatch");
	GENERATED_TEST_ASSERT(
			expect_number_at(&region, result, 1, 20.0) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[1] mismatch");
	GENERATED_TEST_ASSERT(
			expect_number_at(&region, result, 2, 30.0) == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[2] mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

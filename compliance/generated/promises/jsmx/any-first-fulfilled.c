#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/any-first-fulfilled"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t inputs[3];
	jsval_t any;
	jsval_t result;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[0]) == 0 &&
			jsval_promise_reject(&region, inputs[0],
					jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-reject inputs[0]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[1]) == 0 &&
			jsval_promise_resolve(&region, inputs[1],
					jsval_number(42.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[1]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[2]) == 0 &&
			jsval_promise_resolve(&region, inputs[2],
					jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[2]");

	GENERATED_TEST_ASSERT(
			jsval_promise_any(&region, inputs, 3, &any) == 0,
			SUITE, CASE_NAME, "jsval_promise_any failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, any, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "expected FULFILLED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, any, &result) == 0 &&
			result.kind == JSVAL_KIND_NUMBER &&
			result.as.number == 42.0,
			SUITE, CASE_NAME, "expected result == 42");

	return generated_test_pass(SUITE, CASE_NAME);
}

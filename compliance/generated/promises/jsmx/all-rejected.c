#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/all-rejected"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t inputs[3];
	jsval_t all;
	jsval_t reason;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[0]) == 0 &&
			jsval_promise_resolve(&region, inputs[0],
					jsval_number(1.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[0]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[1]) == 0 &&
			jsval_promise_reject(&region, inputs[1],
					jsval_number(99.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-reject inputs[1]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[2]) == 0 &&
			jsval_promise_resolve(&region, inputs[2],
					jsval_number(3.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[2]");

	GENERATED_TEST_ASSERT(
			jsval_promise_all(&region, inputs, 3, &all) == 0,
			SUITE, CASE_NAME, "jsval_promise_all failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "expected REJECTED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, all, &reason) == 0 &&
			reason.kind == JSVAL_KIND_NUMBER &&
			reason.as.number == 99.0,
			SUITE, CASE_NAME, "expected rejection reason == 99");

	return generated_test_pass(SUITE, CASE_NAME);
}

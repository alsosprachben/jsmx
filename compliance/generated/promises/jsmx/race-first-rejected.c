#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/race-first-rejected"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t inputs[2];
	jsval_t race;
	jsval_t reason;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[0]) == 0 &&
			jsval_promise_reject(&region, inputs[0],
					jsval_number(99.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-reject inputs[0]");
	GENERATED_TEST_ASSERT(
			jsval_promise_new(&region, &inputs[1]) == 0 &&
			jsval_promise_resolve(&region, inputs[1],
					jsval_number(7.0)) == 0,
			SUITE, CASE_NAME, "failed to pre-fulfill inputs[1]");

	GENERATED_TEST_ASSERT(
			jsval_promise_race(&region, inputs, 2, &race) == 0,
			SUITE, CASE_NAME, "jsval_promise_race failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, race, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "expected REJECTED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, race, &reason) == 0 &&
			reason.kind == JSVAL_KIND_NUMBER &&
			reason.as.number == 99.0,
			SUITE, CASE_NAME, "expected reason == 99");

	return generated_test_pass(SUITE, CASE_NAME);
}

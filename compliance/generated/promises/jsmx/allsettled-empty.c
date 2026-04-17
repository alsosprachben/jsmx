#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/allsettled-empty"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t all;
	jsval_t result;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_all_settled(&region, NULL, 0, &all) == 0,
			SUITE, CASE_NAME, "jsval_promise_all_settled(n=0) failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "expected FULFILLED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, all, &result) == 0 &&
			result.kind == JSVAL_KIND_ARRAY &&
			jsval_array_length(&region, result) == 0,
			SUITE, CASE_NAME, "expected empty array");

	return generated_test_pass(SUITE, CASE_NAME);
}

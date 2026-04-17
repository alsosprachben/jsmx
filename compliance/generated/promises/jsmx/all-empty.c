#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/all-empty"

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
			jsval_promise_all(&region, NULL, 0, &all) == 0,
			SUITE, CASE_NAME, "jsval_promise_all(n=0) failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0,
			SUITE, CASE_NAME, "jsval_promise_state failed");
	GENERATED_TEST_ASSERT(state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME,
			"expected FULFILLED, got state=%d", (int)state);
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, all, &result) == 0,
			SUITE, CASE_NAME, "jsval_promise_result failed");
	GENERATED_TEST_ASSERT(result.kind == JSVAL_KIND_ARRAY,
			SUITE, CASE_NAME, "expected result.kind == ARRAY");
	GENERATED_TEST_ASSERT(
			jsval_array_length(&region, result) == 0,
			SUITE, CASE_NAME, "expected length 0");

	return generated_test_pass(SUITE, CASE_NAME);
}

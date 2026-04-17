#include <stdint.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/any-empty"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t any;
	jsval_t reason;
	jsval_t errors;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_promise_any(&region, NULL, 0, &any) == 0,
			SUITE, CASE_NAME, "jsval_promise_any(n=0) failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, any, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "expected REJECTED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, any, &reason) == 0 &&
			reason.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME, "expected DOMException reason");
	GENERATED_TEST_ASSERT(
			jsval_dom_exception_errors(&region, reason, &errors) == 0 &&
			errors.kind == JSVAL_KIND_ARRAY &&
			jsval_array_length(&region, errors) == 0,
			SUITE, CASE_NAME, "expected empty errors array");

	return generated_test_pass(SUITE, CASE_NAME);
}

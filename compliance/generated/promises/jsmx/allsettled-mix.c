#include <stdint.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "promises"
#define CASE_NAME "jsmx/allsettled-mix"

static int
expect_entry(jsval_region_t *region, jsval_t array, size_t index,
		const char *expect_status_key, double expect_payload)
{
	jsval_t entry;
	jsval_t payload;

	if (jsval_array_get(region, array, index, &entry) != 0 ||
			entry.kind != JSVAL_KIND_OBJECT) {
		return generated_test_fail(SUITE, CASE_NAME,
				"results[%zu] not an object", index);
	}
	if (jsval_object_get_utf8(region, entry,
			(const uint8_t *)expect_status_key,
			strlen(expect_status_key),
			&payload) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"results[%zu] missing '%s' key", index,
				expect_status_key);
	}
	if (payload.kind != JSVAL_KIND_NUMBER ||
			payload.as.number != expect_payload) {
		return generated_test_fail(SUITE, CASE_NAME,
				"results[%zu].%s expected %g", index,
				expect_status_key, expect_payload);
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
			jsval_promise_all_settled(&region, inputs, 3, &all) == 0,
			SUITE, CASE_NAME, "jsval_promise_all_settled failed");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, all, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "expected FULFILLED");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, all, &result) == 0 &&
			result.kind == JSVAL_KIND_ARRAY &&
			jsval_array_length(&region, result) == 3,
			SUITE, CASE_NAME, "expected 3-element array result");

	GENERATED_TEST_ASSERT(
			expect_entry(&region, result, 0, "value", 1.0)
					== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[0] mismatch");
	GENERATED_TEST_ASSERT(
			expect_entry(&region, result, 1, "reason", 99.0)
					== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[1] mismatch");
	GENERATED_TEST_ASSERT(
			expect_entry(&region, result, 2, "value", 3.0)
					== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "results[2] mismatch");

	return generated_test_pass(SUITE, CASE_NAME);
}

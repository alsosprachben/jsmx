#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-request-json-parity"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t url;
	jsval_t method;
	jsval_t body;
	jsval_t init;
	jsval_t request;
	jsval_t promise;
	jsval_t result;
	jsval_t x_value;
	jsval_t y_value;
	jsval_t y_zero;
	jsval_t y_one;
	jsval_promise_state_t state;
	double number = 0.0;
	int used = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"https://ex.com", 14, &url) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"POST", 4,
				&method) == 0 &&
			jsval_string_new_utf8(&region,
				(const uint8_t *)"{\"x\":1,\"y\":[2,3]}", 17, &body)
				== 0 &&
			jsval_object_new(&region, 4, &init) == 0 &&
			jsval_object_set_utf8(&region, init,
				(const uint8_t *)"method", 6, method) == 0 &&
			jsval_object_set_utf8(&region, init,
				(const uint8_t *)"body", 4, body) == 0 &&
			jsval_request_new(&region, url, init, 1, &request) == 0,
			SUITE, CASE_NAME, "Request setup");

	GENERATED_TEST_ASSERT(
			jsval_request_json(&region, request, &promise) == 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "json() not fulfilled");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, promise, &result) == 0,
			SUITE, CASE_NAME, "json() result unavailable");

	/* obj.x === 1 */
	GENERATED_TEST_ASSERT(
			jsval_object_get_utf8(&region, result,
				(const uint8_t *)"x", 1, &x_value) == 0 &&
			jsval_to_number(&region, x_value, &number) == 0 &&
			number == 1.0,
			SUITE, CASE_NAME, "obj.x != 1");

	/* obj.y is a length-2 array with values [2, 3] */
	GENERATED_TEST_ASSERT(
			jsval_object_get_utf8(&region, result,
				(const uint8_t *)"y", 1, &y_value) == 0 &&
			jsval_array_length(&region, y_value) == 2,
			SUITE, CASE_NAME, "obj.y not length-2 array");
	GENERATED_TEST_ASSERT(
			jsval_array_get(&region, y_value, 0, &y_zero) == 0 &&
			jsval_to_number(&region, y_zero, &number) == 0 &&
			number == 2.0,
			SUITE, CASE_NAME, "obj.y[0] != 2");
	GENERATED_TEST_ASSERT(
			jsval_array_get(&region, y_value, 1, &y_one) == 0 &&
			jsval_to_number(&region, y_one, &number) == 0 &&
			number == 3.0,
			SUITE, CASE_NAME, "obj.y[1] != 3");

	GENERATED_TEST_ASSERT(
			jsval_request_body_used(&region, request, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed true post-consume");

	return generated_test_pass(SUITE, CASE_NAME);
}

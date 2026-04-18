#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-response-array-buffer-parity"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t body;
	jsval_t response;
	jsval_t promise;
	jsval_t result;
	jsval_promise_state_t state;
	uint8_t *bytes;
	size_t len = 0;
	int used = 0;
	static const uint8_t expected[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"hello",
				5, &body) == 0 &&
			jsval_response_new(&region, body, 1, jsval_undefined(), 0,
				&response) == 0,
			SUITE, CASE_NAME, "Response setup");

	/* bodyUsed false before consumption. */
	GENERATED_TEST_ASSERT(
			jsval_response_body_used(&region, response, &used) == 0 &&
			used == 0,
			SUITE, CASE_NAME, "expected bodyUsed == false initially");

	/* arrayBuffer() resolves synchronously for in-memory bodies. */
	GENERATED_TEST_ASSERT(
			jsval_response_array_buffer(&region, response,
				&promise) == 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "arrayBuffer() not fulfilled");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, promise, &result) == 0 &&
			result.kind == JSVAL_KIND_ARRAY_BUFFER,
			SUITE, CASE_NAME, "fulfilled value not ArrayBuffer");
	GENERATED_TEST_ASSERT(
			jsval_array_buffer_bytes_mut(&region, result, &bytes,
				&len) == 0 && len == sizeof(expected) &&
			memcmp(bytes, expected, sizeof(expected)) == 0,
			SUITE, CASE_NAME, "ArrayBuffer bytes mismatch");

	/* bodyUsed true after consumption. */
	GENERATED_TEST_ASSERT(
			jsval_response_body_used(&region, response, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "expected bodyUsed == true after consume");

	return generated_test_pass(SUITE, CASE_NAME);
}

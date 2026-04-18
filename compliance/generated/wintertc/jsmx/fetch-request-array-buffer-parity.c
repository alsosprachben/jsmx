#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-request-array-buffer-parity"

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
	jsval_promise_state_t state;
	uint8_t *bytes;
	size_t len = 0;
	int used = 0;
	static const uint8_t expected[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"https://ex.com", 14, &url) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"POST", 4,
				&method) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"hello", 5,
				&body) == 0 &&
			jsval_object_new(&region, 4, &init) == 0 &&
			jsval_object_set_utf8(&region, init,
				(const uint8_t *)"method", 6, method) == 0 &&
			jsval_object_set_utf8(&region, init,
				(const uint8_t *)"body", 4, body) == 0 &&
			jsval_request_new(&region, url, init, 1, &request) == 0,
			SUITE, CASE_NAME, "Request setup");

	GENERATED_TEST_ASSERT(
			jsval_request_body_used(&region, request, &used) == 0 &&
			used == 0,
			SUITE, CASE_NAME, "bodyUsed false pre-consume");

	GENERATED_TEST_ASSERT(
			jsval_request_array_buffer(&region, request, &promise)
				== 0 &&
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

	GENERATED_TEST_ASSERT(
			jsval_request_body_used(&region, request, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed true post-consume");

	return generated_test_pass(SUITE, CASE_NAME);
}

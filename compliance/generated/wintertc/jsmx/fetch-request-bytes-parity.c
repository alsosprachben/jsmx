#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-request-bytes-parity"

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
	jsval_t backing;
	jsval_promise_state_t state;
	jsval_typed_array_kind_t kind;
	uint8_t *bytes;
	size_t len = 0;
	int used = 0;
	static const uint8_t expected[] = {0x63, 0x61, 0x66, 0xc3, 0xa9};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region,
				(const uint8_t *)"https://ex.com", 14, &url) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"POST", 4,
				&method) == 0 &&
			jsval_string_new_utf8(&region,
				(const uint8_t *)"caf\xc3\xa9", 5, &body) == 0 &&
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
			jsval_request_bytes(&region, request, &promise) == 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "bytes() not fulfilled");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, promise, &result) == 0 &&
			result.kind == JSVAL_KIND_TYPED_ARRAY,
			SUITE, CASE_NAME, "fulfilled value not TypedArray");
	GENERATED_TEST_ASSERT(
			jsval_typed_array_kind(&region, result, &kind) == 0 &&
			kind == JSVAL_TYPED_ARRAY_UINT8,
			SUITE, CASE_NAME, "expected Uint8Array kind");
	GENERATED_TEST_ASSERT(
			jsval_typed_array_length(&region, result) == sizeof(expected),
			SUITE, CASE_NAME, "Uint8Array length mismatch");
	GENERATED_TEST_ASSERT(
			jsval_typed_array_buffer(&region, result, &backing) == 0 &&
			jsval_array_buffer_bytes_mut(&region, backing, &bytes,
				&len) == 0 && len >= sizeof(expected) &&
			memcmp(bytes, expected, sizeof(expected)) == 0,
			SUITE, CASE_NAME, "Uint8Array bytes mismatch");

	GENERATED_TEST_ASSERT(
			jsval_request_body_used(&region, request, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed true post-consume");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "wintertc"
#define CASE_NAME "jsmx/fetch-response-bytes-parity"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t body;
	jsval_t response;
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
				(const uint8_t *)"caf\xc3\xa9", 5, &body) == 0 &&
			jsval_response_new(&region, body, 1, jsval_undefined(), 0,
				&response) == 0,
			SUITE, CASE_NAME, "Response setup");

	GENERATED_TEST_ASSERT(
			jsval_response_body_used(&region, response, &used) == 0 &&
			used == 0,
			SUITE, CASE_NAME, "bodyUsed false pre-consume");

	/* bytes() resolves synchronously with a Uint8Array. */
	GENERATED_TEST_ASSERT(
			jsval_response_bytes(&region, response, &promise) == 0 &&
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
			jsval_response_body_used(&region, response, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed true post-consume");

	return generated_test_pass(SUITE, CASE_NAME);
}

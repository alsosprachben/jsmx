#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/readable-stream-response-body-parity"

static int
expect_done(jsval_region_t *region, jsval_t result, int want_done,
		const char *label)
{
	jsval_t done_value;

	if (jsval_object_get_utf8(region, result,
			(const uint8_t *)"done", 4, &done_value) != 0 ||
			done_value.kind != JSVAL_KIND_BOOL ||
			done_value.as.boolean != want_done) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: done != %d", label, want_done);
	}
	return GENERATED_TEST_PASS;
}

static int
expect_bytes(jsval_region_t *region, jsval_t result,
		const uint8_t *expected, size_t expected_len, const char *label)
{
	jsval_t value;
	jsval_typed_array_kind_t kind;
	jsval_t backing;
	uint8_t *bytes = NULL;
	size_t backing_len = 0;
	size_t len;

	if (jsval_object_get_utf8(region, result,
			(const uint8_t *)"value", 5, &value) != 0 ||
			value.kind != JSVAL_KIND_TYPED_ARRAY) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: value not Uint8Array", label);
	}
	if (jsval_typed_array_kind(region, value, &kind) != 0 ||
			kind != JSVAL_TYPED_ARRAY_UINT8) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: kind != Uint8Array", label);
	}
	len = jsval_typed_array_length(region, value);
	if (len != expected_len) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: length %zu != %zu", label, len, expected_len);
	}
	if (jsval_typed_array_buffer(region, value, &backing) != 0 ||
			jsval_array_buffer_bytes_mut(region, backing, &bytes,
				&backing_len) != 0 ||
			backing_len < expected_len ||
			memcmp(bytes, expected, expected_len) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: bytes mismatch", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t body;
	jsval_t response;
	jsval_t stream;
	jsval_t reader;
	jsval_t promise;
	jsval_t result;
	jsval_t reason;
	jsval_t name_value;
	jsval_promise_state_t state;
	int used = 0;
	size_t nlen = 0;
	uint8_t nbuf[16];
	static const uint8_t source[] = {0x77, 0x6f, 0x72, 0x6c, 0x64};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"world", 5,
				&body) == 0 &&
			jsval_response_new(&region, body, 1, jsval_undefined(), 0,
				&response) == 0,
			SUITE, CASE_NAME, "Response setup");

	GENERATED_TEST_ASSERT(
			jsval_response_body_used(&region, response, &used) == 0 &&
			used == 0,
			SUITE, CASE_NAME, "bodyUsed false pre-access");

	GENERATED_TEST_ASSERT(
			jsval_response_body(&region, response, &stream) == 0 &&
			stream.kind == JSVAL_KIND_READABLE_STREAM,
			SUITE, CASE_NAME, ".body returned non-stream");

	GENERATED_TEST_ASSERT(
			jsval_response_body_used(&region, response, &used) == 0 &&
			used == 1,
			SUITE, CASE_NAME, "bodyUsed true after .body (Phase 1B)");

	/* text() on a used Response rejects with TypeError. */
	GENERATED_TEST_ASSERT(
			jsval_response_text(&region, response, &promise) == 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED &&
			jsval_promise_result(&region, promise, &reason) == 0 &&
			reason.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME, "text() should reject after .body");
	GENERATED_TEST_ASSERT(
			jsval_dom_exception_name(&region, reason, &name_value) == 0 &&
			jsval_string_copy_utf8(&region, name_value, NULL, 0, &nlen)
				== 0 && nlen == 9 && nlen < sizeof(nbuf) &&
			jsval_string_copy_utf8(&region, name_value, nbuf, nlen, NULL)
				== 0 && memcmp(nbuf, "TypeError", 9) == 0,
			SUITE, CASE_NAME, "rejection reason should be TypeError");

	/* Drain the stream: expect "world", then done. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, stream, &reader)
				== 0,
			SUITE, CASE_NAME, "getReader() on body stream");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader, &promise)
				== 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, promise, &result) == 0 &&
			expect_done(&region, result, 0, "read 1")
				== GENERATED_TEST_PASS &&
			expect_bytes(&region, result, source, sizeof(source),
				"read 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 1 should fulfill with bytes");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader, &promise)
				== 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, promise, &result) == 0 &&
			expect_done(&region, result, 1, "read 2")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 2 should be done");

	return generated_test_pass(SUITE, CASE_NAME);
}

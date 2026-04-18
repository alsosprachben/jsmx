#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/readable-stream-from-bytes-parity"

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
	jsval_t stream;
	jsval_t reader;
	jsval_t promise;
	jsval_t result;
	jsval_promise_state_t state;
	static const uint8_t source[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region,
				(const uint8_t *)"hello", 5, &stream) == 0 &&
			stream.kind == JSVAL_KIND_READABLE_STREAM,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, stream, &reader) == 0 &&
			reader.kind == JSVAL_KIND_READABLE_STREAM_READER,
			SUITE, CASE_NAME, "get_reader");

	/* First read: synchronously fulfilled with the full payload. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader, &promise)
				== 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, promise, &result) == 0,
			SUITE, CASE_NAME, "read() 1 not fulfilled");
	GENERATED_TEST_ASSERT(
			expect_done(&region, result, 0, "read 1") == GENERATED_TEST_PASS
			&& expect_bytes(&region, result, source, sizeof(source),
				"read 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 1 bytes");

	/* Second read: EOF. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader, &promise)
				== 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, promise, &result) == 0 &&
			expect_done(&region, result, 1, "read 2")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 2 should be done");

	/* Third read: still EOF. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader, &promise)
				== 0 &&
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, promise, &result) == 0 &&
			expect_done(&region, result, 1, "read 3")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 3 should still be done");

	return generated_test_pass(SUITE, CASE_NAME);
}

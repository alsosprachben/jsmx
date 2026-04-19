#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/pipe-through-text-decoder-parity"

typedef struct fake_body_source_s {
	const uint8_t *data;
	size_t total;
	size_t cursor;
	int chunk_size;
} fake_body_source_t;

static int
fake_body_read(void *userdata, uint8_t *buf, size_t cap, size_t *out_len,
		jsval_body_source_status_t *status_ptr)
{
	fake_body_source_t *src = (fake_body_source_t *)userdata;
	size_t remaining;
	size_t n;

	remaining = src->total - src->cursor;
	n = remaining;
	if (src->chunk_size > 0 && (size_t)src->chunk_size < n) {
		n = (size_t)src->chunk_size;
	}
	if (n > cap) { n = cap; }
	if (n > 0) {
		memcpy(buf, src->data + src->cursor, n);
		src->cursor += n;
	}
	*out_len = n;
	*status_ptr = (src->cursor >= src->total)
			? JSVAL_BODY_SOURCE_STATUS_EOF
			: JSVAL_BODY_SOURCE_STATUS_READY;
	return 0;
}

static void fake_body_close(void *userdata) { (void)userdata; }

static const jsval_body_source_vtable_t fake_body_vtable = {
	fake_body_read,
	fake_body_close,
};

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
	uint8_t storage[131072];
	jsval_region_t region;
	static const uint8_t payload[4] = { 0xCE, 0xB1, 0xCE, 0xB2 };
	fake_body_source_t src = {
		.data = payload, .total = sizeof(payload), .cursor = 0,
		.chunk_size = 3,
	};
	jsval_t readable;
	jsval_t ts;
	jsval_t out_readable;
	jsval_t reader;
	jsval_t pr1, pr2, pr3;
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	static const uint8_t alpha[2] = { 0xCE, 0xB1 };
	static const uint8_t beta[2] = { 0xCE, 0xB2 };

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_source(&region, &fake_body_vtable,
				&src, &readable) == 0,
			SUITE, CASE_NAME, "readable construction (chunk-split source)");

	GENERATED_TEST_ASSERT(
			jsval_text_decoder_stream_new(&region, &ts) == 0,
			SUITE, CASE_NAME, "TextDecoderStream construction");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_pipe_through(&region, readable, ts,
				&out_readable) == 0 &&
			out_readable.kind == JSVAL_KIND_READABLE_STREAM,
			SUITE, CASE_NAME,
			"pipe_through must return a ReadableStream");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, out_readable, &reader)
				== 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pr1)
				== 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pr2)
				== 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pr3)
				== 0,
			SUITE, CASE_NAME, "reader + 3 reads on chained readable");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pr1, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, pr1, &result) == 0 &&
			expect_done(&region, result, 0, "read 1") == GENERATED_TEST_PASS
			&&
			expect_bytes(&region, result, alpha, 2, "read 1")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 1 = alpha (CE B1)");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pr2, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, pr2, &result) == 0 &&
			expect_done(&region, result, 0, "read 2") == GENERATED_TEST_PASS
			&&
			expect_bytes(&region, result, beta, 2, "read 2")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 2 = beta (CE B2)");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pr3, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, pr3, &result) == 0 &&
			expect_done(&region, result, 1, "read 3")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read 3 done");

	return generated_test_pass(SUITE, CASE_NAME);
}

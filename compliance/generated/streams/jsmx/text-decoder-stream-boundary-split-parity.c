#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/text-decoder-stream-boundary-split-parity"

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
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t stream;
	jsval_t readable;
	jsval_t writable;
	jsval_t writer;
	jsval_t reader;
	jsval_t pw1, pw2, pcl;
	jsval_t pr1, pr2, pr3;
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	static const uint8_t part1[3] = { 0xCE, 0xB1, 0xCE };
	static const uint8_t part2[1] = { 0xB2 };
	static const uint8_t alpha[2] = { 0xCE, 0xB1 };
	static const uint8_t beta[2] = { 0xCE, 0xB2 };

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_text_decoder_stream_new(&region, &stream) == 0 &&
			stream.kind == JSVAL_KIND_TRANSFORM_STREAM,
			SUITE, CASE_NAME, "decoder stream construction");
	GENERATED_TEST_ASSERT(
			jsval_transform_stream_readable(&region, stream, &readable)
				== 0 &&
			jsval_transform_stream_writable(&region, stream, &writable)
				== 0 &&
			jsval_writable_stream_get_writer(&region, writable, &writer)
				== 0 &&
			jsval_readable_stream_get_reader(&region, readable, &reader)
				== 0,
			SUITE, CASE_NAME, "writer + reader setup");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer, part1, 3,
				&pw1) == 0 &&
			jsval_writable_stream_writer_write(&region, writer, part2, 1,
				&pw2) == 0 &&
			jsval_writable_stream_writer_close(&region, writer, &pcl)
				== 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pr1)
				== 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pr2)
				== 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pr3)
				== 0,
			SUITE, CASE_NAME, "enqueue writes + reads");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pw1, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, pw2, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, pcl, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "writes + close fulfill");

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

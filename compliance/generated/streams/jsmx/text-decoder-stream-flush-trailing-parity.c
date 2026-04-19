#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/text-decoder-stream-flush-trailing-parity"

static int
collect_bytes(jsval_region_t *region, jsval_t reader,
		uint8_t *out, size_t out_cap, size_t *out_len)
{
	jsval_t promise;
	jsval_t result;
	jsval_t value;
	jsval_t backing;
	jsval_t done_value;
	jsval_promise_state_t state;
	uint8_t *bytes;
	size_t backing_len;
	size_t len;
	size_t total = 0;

	for (;;) {
		if (jsval_readable_stream_reader_read(region, reader, &promise)
				!= 0) {
			return -1;
		}
		if (jsval_promise_state(region, promise, &state) != 0
				|| state != JSVAL_PROMISE_STATE_FULFILLED) {
			return -1;
		}
		if (jsval_promise_result(region, promise, &result) != 0) {
			return -1;
		}
		if (jsval_object_get_utf8(region, result,
				(const uint8_t *)"done", 4, &done_value) != 0) {
			return -1;
		}
		if (done_value.kind == JSVAL_KIND_BOOL && done_value.as.boolean) {
			break;
		}
		if (jsval_object_get_utf8(region, result,
				(const uint8_t *)"value", 5, &value) != 0
				|| value.kind != JSVAL_KIND_TYPED_ARRAY) {
			return -1;
		}
		len = jsval_typed_array_length(region, value);
		if (jsval_typed_array_buffer(region, value, &backing) != 0
				|| jsval_array_buffer_bytes_mut(region, backing, &bytes,
					&backing_len) != 0
				|| backing_len < len
				|| total + len > out_cap) {
			return -1;
		}
		memcpy(out + total, bytes, len);
		total += len;
	}
	*out_len = total;
	return 0;
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
	jsval_t pw, pcl;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	uint8_t out[64];
	size_t out_len = 0;
	static const uint8_t replacement[] = { 0xEF, 0xBF, 0xBD };

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_text_decoder_stream_new(&region, &stream) == 0 &&
			jsval_transform_stream_readable(&region, stream, &readable)
				== 0 &&
			jsval_transform_stream_writable(&region, stream, &writable)
				== 0 &&
			jsval_writable_stream_get_writer(&region, writable, &writer)
				== 0 &&
			jsval_readable_stream_get_reader(&region, readable, &reader)
				== 0,
			SUITE, CASE_NAME, "decoder + writer + reader setup");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"\xCE", 1, &pw) == 0 &&
			jsval_writable_stream_writer_close(&region, writer, &pcl)
				== 0,
			SUITE, CASE_NAME, "write incomplete CE + close");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pw, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, pcl, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "write + close fulfill");

	GENERATED_TEST_ASSERT(
			collect_bytes(&region, reader, out, sizeof(out), &out_len)
				== 0,
			SUITE, CASE_NAME, "collect readable bytes");

	GENERATED_TEST_ASSERT(
			out_len == 3 && memcmp(out, replacement, 3) == 0,
			SUITE, CASE_NAME,
			"flush() must emit U+FFFD for trailing partial sequence");

	return generated_test_pass(SUITE, CASE_NAME);
}

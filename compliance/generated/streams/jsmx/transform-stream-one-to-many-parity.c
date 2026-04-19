#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/transform-stream-one-to-many-parity"

static int
splitter_transform(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len,
		jsval_transform_controller_t *controller)
{
	size_t start = 0;
	size_t i;

	(void)region;
	(void)userdata;
	for (i = 0; i < chunk_len; i++) {
		if (chunk[i] == ',') {
			if (jsval_transform_controller_enqueue(controller,
					chunk + start, i - start) < 0) {
				return -1;
			}
			start = i + 1;
		}
	}
	if (start < chunk_len) {
		if (jsval_transform_controller_enqueue(controller, chunk + start,
				chunk_len - start) < 0) {
			return -1;
		}
	}
	return 0;
}

static const jsval_transformer_vtable_t splitter_vtable = {
	splitter_transform,
	NULL,
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
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t stream;
	jsval_t readable;
	jsval_t writable;
	jsval_t writer;
	jsval_t reader;
	jsval_t pwrite;
	jsval_t pclose;
	jsval_t reads[4];
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	int i;
	const char *expected[3] = { "foo", "bar", "baz" };
	const char *labels[3] = { "read[0]", "read[1]", "read[2]" };

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_transform_stream_new(&region, &splitter_vtable, NULL,
				&stream) == 0 &&
			jsval_transform_stream_readable(&region, stream, &readable)
				== 0 &&
			jsval_transform_stream_writable(&region, stream, &writable)
				== 0,
			SUITE, CASE_NAME, "stream + side accessors");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, writable, &writer)
				== 0 &&
			jsval_readable_stream_get_reader(&region, readable, &reader)
				== 0,
			SUITE, CASE_NAME, "writer + reader");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"foo,bar,baz", 11, &pwrite) == 0 &&
			jsval_writable_stream_writer_close(&region, writer, &pclose)
				== 0,
			SUITE, CASE_NAME, "single write of 'foo,bar,baz' + close");

	for (i = 0; i < 4; i++) {
		GENERATED_TEST_ASSERT(
				jsval_readable_stream_reader_read(&region, reader,
					&reads[i]) == 0,
				SUITE, CASE_NAME, "enqueue read");
	}

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pwrite, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, pclose, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "write + close fulfill");

	for (i = 0; i < 3; i++) {
		GENERATED_TEST_ASSERT(
				jsval_promise_state(&region, reads[i], &state) == 0 &&
				state == JSVAL_PROMISE_STATE_FULFILLED &&
				jsval_promise_result(&region, reads[i], &result) == 0 &&
				expect_done(&region, result, 0, labels[i])
					== GENERATED_TEST_PASS &&
				expect_bytes(&region, result,
					(const uint8_t *)expected[i], 3, labels[i])
					== GENERATED_TEST_PASS,
				SUITE, CASE_NAME, "split chunk content");
	}

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, reads[3], &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, reads[3], &result) == 0 &&
			expect_done(&region, result, 1, "read[3]")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "EOF after 3 split chunks");

	return generated_test_pass(SUITE, CASE_NAME);
}

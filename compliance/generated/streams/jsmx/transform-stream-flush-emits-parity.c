#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/transform-stream-flush-emits-parity"

typedef struct flush_state_s {
	int transform_calls;
	int flush_calls;
} flush_state_t;

static int
flush_transform(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len,
		jsval_transform_controller_t *controller)
{
	flush_state_t *state = (flush_state_t *)userdata;

	(void)region;
	state->transform_calls++;
	return jsval_transform_controller_enqueue(controller, chunk, chunk_len);
}

static int
flush_flush(jsval_region_t *region, void *userdata,
		jsval_transform_controller_t *controller)
{
	flush_state_t *state = (flush_state_t *)userdata;

	(void)region;
	state->flush_calls++;
	return jsval_transform_controller_enqueue(controller,
			(const uint8_t *)"!END", 4);
}

static const jsval_transformer_vtable_t flush_vtable = {
	flush_transform,
	flush_flush,
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
	flush_state_t state_data;
	jsval_t stream;
	jsval_t readable;
	jsval_t writable;
	jsval_t writer;
	jsval_t reader;
	jsval_t pwrite;
	jsval_t pclose;
	jsval_t reads[3];
	jsval_t result;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	int i;

	memset(&state_data, 0, sizeof(state_data));
	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_transform_stream_new(&region, &flush_vtable, &state_data,
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
				(const uint8_t *)"data", 4, &pwrite) == 0 &&
			jsval_writable_stream_writer_close(&region, writer, &pclose)
				== 0,
			SUITE, CASE_NAME, "write 'data' + close");

	for (i = 0; i < 3; i++) {
		GENERATED_TEST_ASSERT(
				jsval_readable_stream_reader_read(&region, reader,
					&reads[i]) == 0,
				SUITE, CASE_NAME, "enqueue read");
	}

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(state_data.transform_calls == 1 &&
			state_data.flush_calls == 1,
			SUITE, CASE_NAME,
			"transform must run once, flush must run once");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, reads[0], &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, reads[0], &result) == 0 &&
			expect_done(&region, result, 0, "read 0") == GENERATED_TEST_PASS
			&&
			expect_bytes(&region, result, (const uint8_t *)"data", 4,
				"read 0") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read[0] == 'data'");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, reads[1], &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, reads[1], &result) == 0 &&
			expect_done(&region, result, 0, "read 1") == GENERATED_TEST_PASS
			&&
			expect_bytes(&region, result, (const uint8_t *)"!END", 4,
				"read 1") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read[1] == '!END' (from flush)");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, reads[2], &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, reads[2], &result) == 0 &&
			expect_done(&region, result, 1, "read 2") == GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read[2] EOF");

	return generated_test_pass(SUITE, CASE_NAME);
}

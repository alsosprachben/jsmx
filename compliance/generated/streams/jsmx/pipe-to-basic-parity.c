#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/pipe-to-basic-parity"

typedef struct capturing_sink_s {
	uint8_t buf[64];
	size_t written;
	int close_calls;
} capturing_sink_t;

static jsval_underlying_sink_status_t
capturing_sink_write(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len, size_t *accepted_len,
		jsval_t *error_value)
{
	capturing_sink_t *sink = (capturing_sink_t *)userdata;
	size_t cap;
	size_t take;

	(void)region;
	(void)error_value;
	cap = sizeof(sink->buf) - sink->written;
	take = chunk_len < cap ? chunk_len : cap;
	if (take > 0) {
		memcpy(sink->buf + sink->written, chunk, take);
		sink->written += take;
	}
	*accepted_len = take;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static jsval_underlying_sink_status_t
capturing_sink_close(jsval_region_t *region, void *userdata,
		jsval_t *error_value)
{
	capturing_sink_t *sink = (capturing_sink_t *)userdata;

	(void)region;
	(void)error_value;
	sink->close_calls++;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static const jsval_underlying_sink_vtable_t capturing_sink_vtable = {
	capturing_sink_write,
	capturing_sink_close,
};

int
main(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	capturing_sink_t sink;
	jsval_t readable;
	jsval_t writable;
	jsval_t pipe_promise;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	memset(&sink, 0, sizeof(sink));
	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region,
				(const uint8_t *)"hello, world", 12, &readable) == 0 &&
			readable.kind == JSVAL_KIND_READABLE_STREAM,
			SUITE, CASE_NAME, "readable construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&capturing_sink_vtable, &sink, &writable) == 0,
			SUITE, CASE_NAME, "writable construction");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_pipe_to(&region, readable, writable,
				&pipe_promise) == 0,
			SUITE, CASE_NAME, "pipe_to scheduled");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pipe_promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "pipe promise must fulfill");

	GENERATED_TEST_ASSERT(
			sink.written == 12 &&
			memcmp(sink.buf, "hello, world", 12) == 0,
			SUITE, CASE_NAME,
			"sink must observe \"hello, world\" in order");

	GENERATED_TEST_ASSERT(sink.close_calls == 1,
			SUITE, CASE_NAME,
			"sink.close must run exactly once");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/writable-stream-basic-write-close-parity"

typedef struct capturing_sink_s {
	uint8_t buf[64];
	size_t written;
	int write_calls;
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
	sink->write_calls++;
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
	uint8_t storage[32768];
	jsval_region_t region;
	capturing_sink_t sink;
	jsval_t stream;
	jsval_t writer;
	jsval_t p1, p2, p3, pclose;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	memset(&sink, 0, sizeof(sink));
	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&capturing_sink_vtable, &sink, &stream) == 0 &&
			stream.kind == JSVAL_KIND_WRITABLE_STREAM,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer) == 0
			&& writer.kind == JSVAL_KIND_WRITABLE_STREAM_DEFAULT_WRITER,
			SUITE, CASE_NAME, "getWriter()");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"foo", 3, &p1) == 0 &&
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"bar", 3, &p2) == 0 &&
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"baz", 3, &p3) == 0 &&
			jsval_writable_stream_writer_close(&region, writer, &pclose)
				== 0,
			SUITE, CASE_NAME, "enqueue writes + close");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, p1, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, p2, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, p3, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_state(&region, pclose, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "all four promises must fulfill");

	GENERATED_TEST_ASSERT(
			sink.written == 9 &&
			memcmp(sink.buf, "foobarbaz", 9) == 0,
			SUITE, CASE_NAME,
			"sink must observe \"foobarbaz\" in order");

	GENERATED_TEST_ASSERT(sink.close_calls == 1,
			SUITE, CASE_NAME, "sink.close must run exactly once");

	return generated_test_pass(SUITE, CASE_NAME);
}

#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/writable-stream-pending-park-parity"

typedef struct pending_sink_s {
	int ready;
	int write_calls;
	int close_calls;
	uint8_t buf[64];
	size_t written;
} pending_sink_t;

static jsval_underlying_sink_status_t
pending_sink_write(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len, size_t *accepted_len,
		jsval_t *error_value)
{
	pending_sink_t *sink = (pending_sink_t *)userdata;

	(void)region;
	(void)error_value;
	sink->write_calls++;
	if (!sink->ready) {
		*accepted_len = 0;
		return JSVAL_UNDERLYING_SINK_STATUS_PENDING;
	}
	memcpy(sink->buf + sink->written, chunk, chunk_len);
	sink->written += chunk_len;
	*accepted_len = chunk_len;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static jsval_underlying_sink_status_t
pending_sink_close(jsval_region_t *region, void *userdata,
		jsval_t *error_value)
{
	pending_sink_t *sink = (pending_sink_t *)userdata;

	(void)region;
	(void)error_value;
	sink->close_calls++;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static const jsval_underlying_sink_vtable_t pending_sink_vtable = {
	pending_sink_write,
	pending_sink_close,
};

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	pending_sink_t sink;
	jsval_t stream;
	jsval_t writer;
	jsval_t promise;
	jsval_promise_state_t state;
	jsval_off_t sink_off = 0;
	jsmethod_error_t error;

	memset(&sink, 0, sizeof(sink));
	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&pending_sink_vtable, &sink, &stream) == 0,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_sink_off(&region, stream, &sink_off)
				== 0 && sink_off != 0,
			SUITE, CASE_NAME, "sink_off accessor");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer) == 0,
			SUITE, CASE_NAME, "getWriter()");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"hi", 2, &promise) == 0,
			SUITE, CASE_NAME, "enqueue write");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "first drain");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME,
			"write promise must remain pending while sink PENDING");
	GENERATED_TEST_ASSERT(sink.write_calls == 1,
			SUITE, CASE_NAME,
			"sink.write must be called exactly once before notify");

	/* Drain again must NOT busy-loop: pump is parked, no microtask. */
	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "second drain (must be a no-op)");
	GENERATED_TEST_ASSERT(sink.write_calls == 1,
			SUITE, CASE_NAME,
			"sink.write must NOT be re-called without notify "
			"(busy-loop regression guard)");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME,
			"promise must still be pending after drain");

	/* Flip ready and notify. */
	sink.ready = 1;
	GENERATED_TEST_ASSERT(
			jsval_underlying_sink_notify(&region, sink_off) == 0,
			SUITE, CASE_NAME, "notify");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "post-notify drain");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "promise must fulfill after notify");
	GENERATED_TEST_ASSERT(
			sink.written == 2 && memcmp(sink.buf, "hi", 2) == 0,
			SUITE, CASE_NAME, "sink must observe \"hi\"");

	return generated_test_pass(SUITE, CASE_NAME);
}

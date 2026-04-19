#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/writable-stream-abort-parity"

typedef struct parking_sink_s {
	int write_calls;
	int close_calls;
} parking_sink_t;

static jsval_underlying_sink_status_t
parking_sink_write(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len, size_t *accepted_len,
		jsval_t *error_value)
{
	parking_sink_t *sink = (parking_sink_t *)userdata;

	(void)region;
	(void)chunk;
	(void)chunk_len;
	(void)error_value;
	sink->write_calls++;
	*accepted_len = 0;
	return JSVAL_UNDERLYING_SINK_STATUS_PENDING;
}

static jsval_underlying_sink_status_t
parking_sink_close(jsval_region_t *region, void *userdata,
		jsval_t *error_value)
{
	parking_sink_t *sink = (parking_sink_t *)userdata;

	(void)region;
	(void)error_value;
	sink->close_calls++;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static const jsval_underlying_sink_vtable_t parking_sink_vtable = {
	parking_sink_write,
	parking_sink_close,
};

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	parking_sink_t sink;
	jsval_t stream;
	jsval_t writer;
	jsval_t pwrite;
	jsval_t pabort;
	jsval_t ppost;
	jsval_t reason_value;
	jsval_t reason_name;
	jsval_t name_v;
	jsval_t msg_v;
	jsval_promise_state_t state;
	jsval_t result;
	uint8_t name_buf[16];
	size_t nlen = 0;
	jsmethod_error_t error;

	memset(&sink, 0, sizeof(sink));
	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&parking_sink_vtable, &sink, &stream) == 0,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer) == 0,
			SUITE, CASE_NAME, "getWriter()");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"x", 1, &pwrite) == 0,
			SUITE, CASE_NAME, "enqueue write");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain (write parks)");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pwrite, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_PENDING,
			SUITE, CASE_NAME, "write must park while sink PENDING");

	/* Build a TypeError DOMException as the abort reason. */
	GENERATED_TEST_ASSERT(
			jsval_string_new_utf8(&region, (const uint8_t *)"TypeError",
				9, &name_v) == 0 &&
			jsval_string_new_utf8(&region, (const uint8_t *)"nope", 4,
				&msg_v) == 0 &&
			jsval_dom_exception_new(&region, name_v, msg_v, &reason_value)
				== 0,
			SUITE, CASE_NAME, "construct TypeError abort reason");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_abort(&region, stream, reason_value,
				&pabort) == 0,
			SUITE, CASE_NAME, "abort()");

	/* abort() fulfills with undefined. */
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pabort, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, pabort, &result) == 0 &&
			result.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME,
			"abort() must fulfill with undefined");

	/* Queued write rejects with the abort reason. */
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pwrite, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED &&
			jsval_promise_result(&region, pwrite, &result) == 0 &&
			result.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME,
			"queued write must reject as DOMException");
	GENERATED_TEST_ASSERT(
			jsval_dom_exception_name(&region, result, &reason_name) == 0 &&
			jsval_string_copy_utf8(&region, reason_name, NULL, 0, &nlen)
				== 0 && nlen == 9 && nlen < sizeof(name_buf) &&
			jsval_string_copy_utf8(&region, reason_name, name_buf, nlen,
				NULL) == 0 &&
			memcmp(name_buf, "TypeError", 9) == 0,
			SUITE, CASE_NAME,
			"reject reason name must be \"TypeError\"");

	/* Sink's close ran. */
	GENERATED_TEST_ASSERT(sink.close_calls == 1,
			SUITE, CASE_NAME,
			"sink.close must run exactly once on abort");

	/* Post-abort write rejects synchronously. */
	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"y", 1, &ppost) == 0 &&
			jsval_promise_state(&region, ppost, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED &&
			jsval_promise_result(&region, ppost, &result) == 0 &&
			result.kind == JSVAL_KIND_DOM_EXCEPTION,
			SUITE, CASE_NAME,
			"post-abort write must reject synchronously");

	return generated_test_pass(SUITE, CASE_NAME);
}

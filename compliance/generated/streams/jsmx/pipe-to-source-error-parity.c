#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/pipe-to-source-error-parity"

typedef struct fail_after_source_s {
	int reads;
	int fail_after;
	const uint8_t *data;
	size_t total;
	size_t cursor;
	int chunk_size;
} fail_after_source_t;

static int
fail_after_read(void *userdata, uint8_t *buf, size_t cap, size_t *out_len,
		jsval_body_source_status_t *status_ptr)
{
	fail_after_source_t *src = (fail_after_source_t *)userdata;
	size_t remaining;
	size_t n;

	src->reads++;
	if (src->fail_after >= 0 && src->reads > src->fail_after) {
		*out_len = 0;
		*status_ptr = JSVAL_BODY_SOURCE_STATUS_ERROR;
		return 0;
	}
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

static void fail_after_close(void *userdata) { (void)userdata; }

static const jsval_body_source_vtable_t fail_after_vtable = {
	fail_after_read,
	fail_after_close,
};

static jsval_underlying_sink_status_t
noop_sink_write(jsval_region_t *region, void *userdata, const uint8_t *chunk,
		size_t chunk_len, size_t *accepted_len, jsval_t *error_value)
{
	(void)region;
	(void)userdata;
	(void)chunk;
	(void)error_value;
	*accepted_len = chunk_len;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static jsval_underlying_sink_status_t
noop_sink_close(jsval_region_t *region, void *userdata, jsval_t *error_value)
{
	(void)region;
	(void)userdata;
	(void)error_value;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static const jsval_underlying_sink_vtable_t noop_sink_vtable = {
	noop_sink_write,
	noop_sink_close,
};

int
main(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	static const uint8_t payload[] = "abc";
	fail_after_source_t src = {
		.reads = 0, .fail_after = 1,
		.data = payload, .total = 3, .cursor = 0, .chunk_size = 1,
	};
	jsval_t readable;
	jsval_t writable;
	jsval_t pipe_promise;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_source(&region, &fail_after_vtable,
				&src, &readable) == 0,
			SUITE, CASE_NAME, "readable construction (fail-after-1)");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region, &noop_sink_vtable,
				NULL, &writable) == 0,
			SUITE, CASE_NAME, "writable construction (noop sink)");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_pipe_to(&region, readable, writable,
				&pipe_promise) == 0,
			SUITE, CASE_NAME, "pipe_to scheduled");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pipe_promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME,
			"pipe promise must reject when source errors");

	return generated_test_pass(SUITE, CASE_NAME);
}

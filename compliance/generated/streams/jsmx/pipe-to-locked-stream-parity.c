#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/pipe-to-locked-stream-parity"

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
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t readable;
	jsval_t writable;
	jsval_t held_reader;
	jsval_t pipe_promise;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region,
				(const uint8_t *)"x", 1, &readable) == 0,
			SUITE, CASE_NAME, "readable construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region, &noop_sink_vtable,
				NULL, &writable) == 0,
			SUITE, CASE_NAME, "writable construction");

	/* Pre-lock the readable side. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, readable, &held_reader)
				== 0 &&
			held_reader.kind == JSVAL_KIND_READABLE_STREAM_READER,
			SUITE, CASE_NAME, "pre-lock readable via getReader");

	/* pipe_to must fail synchronously with EBUSY before scheduling. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_pipe_to(&region, readable, writable,
				&pipe_promise) < 0 && errno == EBUSY,
			SUITE, CASE_NAME,
			"pipe_to on locked readable must fail with EBUSY");

	return generated_test_pass(SUITE, CASE_NAME);
}

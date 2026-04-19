#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/writable-stream-locked-release-parity"

static jsval_underlying_sink_status_t
noop_sink_write(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len, size_t *accepted_len,
		jsval_t *error_value)
{
	(void)region;
	(void)userdata;
	(void)chunk;
	(void)error_value;
	*accepted_len = chunk_len;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static jsval_underlying_sink_status_t
noop_sink_close(jsval_region_t *region, void *userdata,
		jsval_t *error_value)
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
	jsval_t stream;
	jsval_t writer1;
	jsval_t writer2;
	jsval_t promise;
	int locked = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&noop_sink_vtable, NULL, &stream) == 0,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_locked(&region, stream, &locked) == 0 &&
			locked == 0,
			SUITE, CASE_NAME, "locked should be false initially");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer1)
				== 0 &&
			writer1.kind == JSVAL_KIND_WRITABLE_STREAM_DEFAULT_WRITER,
			SUITE, CASE_NAME, "first getWriter() should succeed");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_locked(&region, stream, &locked) == 0 &&
			locked == 1,
			SUITE, CASE_NAME, "locked should be true after getWriter()");

	/* Second getWriter() on locked stream fails with EBUSY. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer2)
				< 0 && errno == EBUSY,
			SUITE, CASE_NAME,
			"second getWriter() should fail with EBUSY");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_release_lock(&region, writer1)
				== 0,
			SUITE, CASE_NAME, "releaseLock() should succeed");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_locked(&region, stream, &locked) == 0 &&
			locked == 0,
			SUITE, CASE_NAME, "locked should be false after release");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer2)
				== 0 &&
			writer2.kind == JSVAL_KIND_WRITABLE_STREAM_DEFAULT_WRITER,
			SUITE, CASE_NAME,
			"getWriter() after release should succeed");

	/* Writer methods on a released writer must fail. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer1,
				(const uint8_t *)"x", 1, &promise) < 0 && errno == EINVAL,
			SUITE, CASE_NAME,
			"write() on released writer should fail with EINVAL");

	return generated_test_pass(SUITE, CASE_NAME);
}

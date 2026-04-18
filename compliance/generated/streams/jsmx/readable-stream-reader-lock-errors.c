#include <errno.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/readable-stream-reader-lock-errors"

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t stream;
	jsval_t reader1;
	jsval_t reader2;
	int locked = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region,
				(const uint8_t *)"abc", 3, &stream) == 0,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_locked(&region, stream, &locked) == 0 &&
			locked == 0,
			SUITE, CASE_NAME, "locked should be false initially");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, stream, &reader1)
				== 0 &&
			reader1.kind == JSVAL_KIND_READABLE_STREAM_READER,
			SUITE, CASE_NAME, "first getReader() should succeed");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_locked(&region, stream, &locked) == 0 &&
			locked == 1,
			SUITE, CASE_NAME, "locked should be true after getReader()");

	/* Second getReader() on locked stream fails with EBUSY. */
	errno = 0;
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, stream, &reader2)
				< 0 && errno == EBUSY,
			SUITE, CASE_NAME,
			"second getReader() should fail with EBUSY");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_release_lock(&region, reader1)
				== 0,
			SUITE, CASE_NAME, "releaseLock() should succeed");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_locked(&region, stream, &locked) == 0 &&
			locked == 0,
			SUITE, CASE_NAME, "locked should be false after release");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_get_reader(&region, stream, &reader2)
				== 0 &&
			reader2.kind == JSVAL_KIND_READABLE_STREAM_READER,
			SUITE, CASE_NAME,
			"getReader() after release should succeed");

	return generated_test_pass(SUITE, CASE_NAME);
}

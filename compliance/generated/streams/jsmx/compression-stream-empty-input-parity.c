#include <stdlib.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/compression-stream-empty-input-parity"

static int
collect_bytes(jsval_region_t *region, jsval_t reader, uint8_t *out,
		size_t out_cap, size_t *out_len)
{
	jsval_t promise;
	jsval_t result;
	jsval_t value;
	jsval_t backing;
	jsval_t done_value;
	jsval_promise_state_t state;
	uint8_t *bytes;
	size_t backing_len;
	size_t len;
	size_t total = 0;
	jsmethod_error_t error;

	for (;;) {
		if (jsval_readable_stream_reader_read(region, reader, &promise)
				!= 0) {
			return -1;
		}
		memset(&error, 0, sizeof(error));
		if (jsval_microtask_drain(region, &error) != 0) {
			return -1;
		}
		if (jsval_promise_state(region, promise, &state) != 0
				|| state != JSVAL_PROMISE_STATE_FULFILLED) {
			return -1;
		}
		if (jsval_promise_result(region, promise, &result) != 0) {
			return -1;
		}
		if (jsval_object_get_utf8(region, result,
				(const uint8_t *)"done", 4, &done_value) != 0) {
			return -1;
		}
		if (done_value.kind == JSVAL_KIND_BOOL && done_value.as.boolean) {
			break;
		}
		if (jsval_object_get_utf8(region, result,
				(const uint8_t *)"value", 5, &value) != 0
				|| value.kind != JSVAL_KIND_TYPED_ARRAY) {
			return -1;
		}
		len = jsval_typed_array_length(region, value);
		if (jsval_typed_array_buffer(region, value, &backing) != 0
				|| jsval_array_buffer_bytes_mut(region, backing, &bytes,
					&backing_len) != 0
				|| backing_len < len
				|| total + len > out_cap) {
			return -1;
		}
		memcpy(out + total, bytes, len);
		total += len;
	}
	*out_len = total;
	return 0;
}

int
main(void)
{
	const size_t storage_len = 1 * 1024 * 1024;
	uint8_t *storage;
	uint8_t compressed[128];
	uint8_t decompressed[16];
	size_t compressed_len = 0;
	size_t decompressed_len = 1; /* sentinel: must become 0 */
	jsval_region_t region;
	jsval_t readable;
	jsval_t cs;
	jsval_t out_readable;
	jsval_t reader;

	storage = malloc(storage_len);
	GENERATED_TEST_ASSERT(storage != NULL, SUITE, CASE_NAME,
			"allocate 1 MB arena");

	/* Compress empty input -- the compressor's flush() path still
	 * emits a small gzip envelope. */
	jsval_region_init(&region, storage, storage_len);
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region, NULL, 0,
				&readable) == 0 &&
			jsval_compression_stream_new(&region,
				JSVAL_COMPRESSION_FORMAT_GZIP, &cs) == 0 &&
			jsval_readable_stream_pipe_through(&region, readable, cs,
				&out_readable) == 0 &&
			jsval_readable_stream_get_reader(&region, out_readable, &reader)
				== 0,
			SUITE, CASE_NAME, "empty readable + compress setup");

	GENERATED_TEST_ASSERT(
			collect_bytes(&region, reader, compressed, sizeof(compressed),
				&compressed_len) == 0,
			SUITE, CASE_NAME, "collect compressed envelope");

	GENERATED_TEST_ASSERT(compressed_len > 0, SUITE, CASE_NAME,
			"compressed envelope must be non-empty for empty input");

	/* Decompress the envelope -- must yield empty output. */
	jsval_region_init(&region, storage, storage_len);
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region, compressed,
				compressed_len, &readable) == 0 &&
			jsval_decompression_stream_new(&region,
				JSVAL_COMPRESSION_FORMAT_GZIP, &cs) == 0 &&
			jsval_readable_stream_pipe_through(&region, readable, cs,
				&out_readable) == 0 &&
			jsval_readable_stream_get_reader(&region, out_readable, &reader)
				== 0,
			SUITE, CASE_NAME, "decompress pipeThrough setup");

	GENERATED_TEST_ASSERT(
			collect_bytes(&region, reader, decompressed,
				sizeof(decompressed), &decompressed_len) == 0,
			SUITE, CASE_NAME, "collect decompressed bytes");

	GENERATED_TEST_ASSERT(decompressed_len == 0, SUITE, CASE_NAME,
			"decompressed output must be empty for empty input");

	free(storage);
	return generated_test_pass(SUITE, CASE_NAME);
}

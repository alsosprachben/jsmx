#include <stdlib.h>
#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/decompression-stream-invalid-input-parity"

static int
expect_typeerror(jsval_region_t *region, jsval_t reason, const char *label)
{
	jsval_t name_value;
	uint8_t name_buf[16];
	size_t nlen = 0;

	if (reason.kind != JSVAL_KIND_DOM_EXCEPTION) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: rejection reason kind != DOM_EXCEPTION", label);
	}
	if (jsval_dom_exception_name(region, reason, &name_value) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: dom_exception_name failed", label);
	}
	if (jsval_string_copy_utf8(region, name_value, NULL, 0, &nlen) != 0
			|| nlen != 9 || nlen >= sizeof(name_buf)) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: name length wrong", label);
	}
	if (jsval_string_copy_utf8(region, name_value, name_buf, nlen, NULL)
				!= 0 ||
			memcmp(name_buf, "TypeError", 9) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: name != TypeError", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	static const uint8_t garbage[] =
			{ 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
	const size_t storage_len = 1 * 1024 * 1024;
	uint8_t *storage;
	jsval_region_t region;
	jsval_t readable;
	jsval_t ds;
	jsval_t out_readable;
	jsval_t reader;
	jsval_t promise;
	jsval_t reason;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	storage = malloc(storage_len);
	GENERATED_TEST_ASSERT(storage != NULL, SUITE, CASE_NAME,
			"allocate 1 MB arena");
	jsval_region_init(&region, storage, storage_len);

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region, garbage,
				sizeof(garbage), &readable) == 0 &&
			jsval_decompression_stream_new(&region,
				JSVAL_COMPRESSION_FORMAT_GZIP, &ds) == 0 &&
			jsval_readable_stream_pipe_through(&region, readable, ds,
				&out_readable) == 0 &&
			jsval_readable_stream_get_reader(&region, out_readable, &reader)
				== 0,
			SUITE, CASE_NAME,
			"readable + decompression stream + pipeThrough + reader");

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader, &promise)
				== 0,
			SUITE, CASE_NAME, "schedule first read");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME,
			"read promise must reject on invalid gzip input");

	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, promise, &reason) == 0 &&
			expect_typeerror(&region, reason, "read rejection")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"rejection reason must be TypeError DOMException");

	free(storage);
	return generated_test_pass(SUITE, CASE_NAME);
}

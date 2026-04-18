#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/readable-stream-cancel-parity"

static int
expect_done(jsval_region_t *region, jsval_t result, int want_done,
		const char *label)
{
	jsval_t done_value;

	if (jsval_object_get_utf8(region, result,
			(const uint8_t *)"done", 4, &done_value) != 0 ||
			done_value.kind != JSVAL_KIND_BOOL ||
			done_value.as.boolean != want_done) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: done != %d", label, want_done);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[16384];
	jsval_region_t region;
	jsval_t stream;
	jsval_t reader;
	jsval_t cancel_promise;
	jsval_t read_promise;
	jsval_t result;
	jsval_t cancel_value;
	jsval_promise_state_t state;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region,
				(const uint8_t *)"abcdef", 6, &stream) == 0 &&
			jsval_readable_stream_get_reader(&region, stream, &reader)
				== 0,
			SUITE, CASE_NAME, "stream + reader setup");

	/* cancel() fulfills with undefined. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_cancel(&region, stream,
				jsval_undefined(), &cancel_promise) == 0 &&
			jsval_promise_state(&region, cancel_promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED,
			SUITE, CASE_NAME, "cancel() not fulfilled");
	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, cancel_promise, &cancel_value)
				== 0 && cancel_value.kind == JSVAL_KIND_UNDEFINED,
			SUITE, CASE_NAME, "cancel() should resolve to undefined");

	/* read() after cancel: done:true (bytes were dropped). */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader,
				&read_promise) == 0 &&
			jsval_promise_state(&region, read_promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, read_promise, &result) == 0 &&
			expect_done(&region, result, 1, "read after cancel")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "read after cancel should be done");

	/* Repeated read after cancel stays done. */
	GENERATED_TEST_ASSERT(
			jsval_readable_stream_reader_read(&region, reader,
				&read_promise) == 0 &&
			jsval_promise_state(&region, read_promise, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_FULFILLED &&
			jsval_promise_result(&region, read_promise, &result) == 0 &&
			expect_done(&region, result, 1, "repeated read")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "repeated read should stay done");

	return generated_test_pass(SUITE, CASE_NAME);
}

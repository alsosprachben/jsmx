#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/pipe-to-sink-error-parity"

static jsval_underlying_sink_status_t
erroring_sink_write(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len, size_t *accepted_len,
		jsval_t *error_value)
{
	jsval_t name_v;
	jsval_t msg_v;

	(void)userdata;
	(void)chunk;
	(void)chunk_len;
	*accepted_len = 0;
	*error_value = jsval_undefined();
	if (jsval_string_new_utf8(region, (const uint8_t *)"TypeError", 9,
				&name_v) == 0 &&
			jsval_string_new_utf8(region, (const uint8_t *)"nope", 4,
				&msg_v) == 0) {
		(void)jsval_dom_exception_new(region, name_v, msg_v, error_value);
	}
	return JSVAL_UNDERLYING_SINK_STATUS_ERROR;
}

static jsval_underlying_sink_status_t
erroring_sink_close(jsval_region_t *region, void *userdata,
		jsval_t *error_value)
{
	(void)region;
	(void)userdata;
	(void)error_value;
	return JSVAL_UNDERLYING_SINK_STATUS_READY;
}

static const jsval_underlying_sink_vtable_t erroring_sink_vtable = {
	erroring_sink_write,
	erroring_sink_close,
};

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
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t readable;
	jsval_t writable;
	jsval_t pipe_promise;
	jsval_t reason;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_readable_stream_new_from_bytes(&region,
				(const uint8_t *)"xyz", 3, &readable) == 0,
			SUITE, CASE_NAME, "readable construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&erroring_sink_vtable, NULL, &writable) == 0,
			SUITE, CASE_NAME, "erroring writable construction");

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
			"pipe promise must reject when sink errors");

	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, pipe_promise, &reason) == 0 &&
			expect_typeerror(&region, reason, "pipe rejection")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"rejection reason must be TypeError DOMException");

	return generated_test_pass(SUITE, CASE_NAME);
}

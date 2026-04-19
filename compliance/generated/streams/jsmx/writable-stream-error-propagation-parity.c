#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/writable-stream-error-propagation-parity"

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
			jsval_string_new_utf8(region, (const uint8_t *)"boom", 4,
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
				"%s: rejection name length wrong", label);
	}
	if (jsval_string_copy_utf8(region, name_value, name_buf, nlen, NULL)
				!= 0 ||
			memcmp(name_buf, "TypeError", 9) != 0) {
		return generated_test_fail(SUITE, CASE_NAME,
				"%s: rejection name != TypeError", label);
	}
	return GENERATED_TEST_PASS;
}

int
main(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t stream;
	jsval_t writer;
	jsval_t p1, p2, p3, pclose;
	jsval_t reason;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_new_from_sink(&region,
				&erroring_sink_vtable, NULL, &stream) == 0,
			SUITE, CASE_NAME, "stream construction");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, stream, &writer) == 0,
			SUITE, CASE_NAME, "getWriter()");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"x", 1, &p1) == 0 &&
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"y", 1, &p2) == 0 &&
			jsval_writable_stream_writer_close(&region, writer, &pclose)
				== 0,
			SUITE, CASE_NAME, "enqueue writes + close");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, p1, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED &&
			jsval_promise_state(&region, p2, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED &&
			jsval_promise_state(&region, pclose, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME,
			"queued writes + close must all reject");

	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, p1, &reason) == 0 &&
			expect_typeerror(&region, reason, "p1")
				== GENERATED_TEST_PASS &&
			jsval_promise_result(&region, p2, &reason) == 0 &&
			expect_typeerror(&region, reason, "p2")
				== GENERATED_TEST_PASS &&
			jsval_promise_result(&region, pclose, &reason) == 0 &&
			expect_typeerror(&region, reason, "pclose")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"each rejection reason must be TypeError DOMException");

	/* Post-error write must reject synchronously from stored error. */
	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"z", 1, &p3) == 0 &&
			jsval_promise_state(&region, p3, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED &&
			jsval_promise_result(&region, p3, &reason) == 0 &&
			expect_typeerror(&region, reason, "p3")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME,
			"post-error write must reject synchronously");

	return generated_test_pass(SUITE, CASE_NAME);
}

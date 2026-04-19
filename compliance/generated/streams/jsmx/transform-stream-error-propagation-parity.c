#include <string.h>

#include "compliance/generated/test_contract.h"
#include "jsval.h"

#define SUITE "streams"
#define CASE_NAME "jsmx/transform-stream-error-propagation-parity"

static int
erroring_transform(jsval_region_t *region, void *userdata,
		const uint8_t *chunk, size_t chunk_len,
		jsval_transform_controller_t *controller)
{
	jsval_t name_v;
	jsval_t msg_v;
	jsval_t reason = jsval_undefined();

	(void)userdata;
	(void)chunk;
	(void)chunk_len;
	if (jsval_string_new_utf8(region, (const uint8_t *)"TypeError", 9,
				&name_v) == 0 &&
			jsval_string_new_utf8(region, (const uint8_t *)"bad chunk", 9,
				&msg_v) == 0) {
		(void)jsval_dom_exception_new(region, name_v, msg_v, &reason);
	}
	(void)jsval_transform_controller_error(controller, reason);
	return -1;
}

static const jsval_transformer_vtable_t erroring_vtable = {
	erroring_transform,
	NULL,
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
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t stream;
	jsval_t readable;
	jsval_t writable;
	jsval_t writer;
	jsval_t reader;
	jsval_t pwrite;
	jsval_t pread;
	jsval_t ppost;
	jsval_t reason;
	jsval_promise_state_t state;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));

	GENERATED_TEST_ASSERT(
			jsval_transform_stream_new(&region, &erroring_vtable, NULL,
				&stream) == 0 &&
			jsval_transform_stream_readable(&region, stream, &readable)
				== 0 &&
			jsval_transform_stream_writable(&region, stream, &writable)
				== 0,
			SUITE, CASE_NAME, "stream + side accessors");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_get_writer(&region, writable, &writer)
				== 0 &&
			jsval_readable_stream_get_reader(&region, readable, &reader)
				== 0,
			SUITE, CASE_NAME, "writer + reader");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"x", 1, &pwrite) == 0 &&
			jsval_readable_stream_reader_read(&region, reader, &pread)
				== 0,
			SUITE, CASE_NAME, "enqueue write + read");

	memset(&error, 0, sizeof(error));
	GENERATED_TEST_ASSERT(jsval_microtask_drain(&region, &error) == 0,
			SUITE, CASE_NAME, "drain microtasks");

	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pwrite, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "writer.write must reject");
	GENERATED_TEST_ASSERT(
			jsval_promise_state(&region, pread, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME, "reader.read must reject");

	GENERATED_TEST_ASSERT(
			jsval_promise_result(&region, pread, &reason) == 0 &&
			expect_typeerror(&region, reason, "pread")
				== GENERATED_TEST_PASS,
			SUITE, CASE_NAME, "reader rejection reason == TypeError");

	GENERATED_TEST_ASSERT(
			jsval_writable_stream_writer_write(&region, writer,
				(const uint8_t *)"y", 1, &ppost) == 0 &&
			jsval_promise_state(&region, ppost, &state) == 0 &&
			state == JSVAL_PROMISE_STATE_REJECTED,
			SUITE, CASE_NAME,
			"post-error write must reject synchronously");

	return generated_test_pass(SUITE, CASE_NAME);
}

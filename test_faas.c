#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsval.h"
#include "runtime_modules/shared/faas_bridge.h"

/* ---------- Fake body source (copied pattern from test_jsval) ---------- */

typedef struct faas_fake_body_s {
	const uint8_t *data;
	size_t total;
	size_t cursor;
	int chunk_size;
	int close_calls;
} faas_fake_body_t;

static int faas_fake_body_read(void *userdata, uint8_t *buf, size_t cap,
		size_t *out_len, jsval_body_source_status_t *status_ptr)
{
	faas_fake_body_t *src = (faas_fake_body_t *)userdata;
	size_t remaining = src->total - src->cursor;
	size_t n = remaining;

	if (src->chunk_size > 0 && (size_t)src->chunk_size < n) {
		n = (size_t)src->chunk_size;
	}
	if (n > cap) { n = cap; }
	if (n > 0) {
		memcpy(buf, src->data + src->cursor, n);
		src->cursor += n;
	}
	*out_len = n;
	*status_ptr = src->cursor >= src->total
			? JSVAL_BODY_SOURCE_STATUS_EOF
			: JSVAL_BODY_SOURCE_STATUS_READY;
	return 0;
}

static void faas_fake_body_close(void *userdata)
{
	faas_fake_body_t *src = (faas_fake_body_t *)userdata;
	src->close_calls++;
}

static const jsval_body_source_vtable_t faas_fake_vtable = {
	faas_fake_body_read,
	faas_fake_body_close,
};

/* ---------- Helpers ---------- */

static jsval_t str(jsval_region_t *region, const char *s)
{
	jsval_t v;
	assert(jsval_string_new_utf8(region, (const uint8_t *)s, strlen(s), &v)
			== 0);
	return v;
}

static int contains_bytes(const uint8_t *haystack, size_t hsz,
		const char *needle)
{
	size_t nsz = strlen(needle);
	size_t i;
	if (nsz > hsz) { return 0; }
	for (i = 0; i + nsz <= hsz; i++) {
		if (memcmp(haystack + i, needle, nsz) == 0) { return 1; }
	}
	return 0;
}

static int count_substrings(const uint8_t *haystack, size_t hsz,
		const char *needle)
{
	size_t nsz = strlen(needle);
	size_t i;
	int count = 0;
	if (nsz == 0 || nsz > hsz) { return 0; }
	for (i = 0; i + nsz <= hsz; ) {
		if (memcmp(haystack + i, needle, nsz) == 0) {
			count++;
			i += nsz;
		} else {
			i++;
		}
	}
	return count;
}

/* ---------- Test 1: plain handler, no body ---------- */

static int plain_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t response;
	jsval_t init;
	jsval_t promise;

	(void)request;
	assert(jsval_object_new(region, 2, &init) == 0);
	assert(jsval_object_set_utf8(region, init,
			(const uint8_t *)"status", 6, jsval_number(200.0)) == 0);
	assert(jsval_object_set_utf8(region, init,
			(const uint8_t *)"statusText", 10, str(region, "OK")) == 0);
	assert(jsval_response_new(region, str(region, "hi"), 1, init, 1,
			&response) == 0);
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_promise_resolve(region, promise, response) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_plain_handler(void)
{
	uint8_t storage[65536];
	uint8_t out[512];
	jsval_region_t region;
	jsval_t headers;
	jsval_t url;
	jsval_t request;
	jsval_t response_promise;
	jsval_t response;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	size_t out_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST, &headers)
			== 0);
	url = str(&region, "https://ex.com/");
	assert(jsval_request_new_from_parts(&region, jsval_undefined(), url,
			headers, NULL, NULL, SIZE_MAX, &request) == 0);

	assert(plain_handler(&region, request, &response_promise) == 0);
	memset(&error, 0, sizeof(error));
	assert(faas_drain_until_settled(&region, response_promise, &error) == 0);
	assert(jsval_promise_state(&region, response_promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, response_promise, &response) == 0);
	assert(response.kind == JSVAL_KIND_RESPONSE);

	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) == 0);
	assert(out_len > 0);
	assert(contains_bytes(out, out_len, "HTTP/1.1 200 OK\r\n"));
	assert(contains_bytes(out, out_len, "Content-Length: 2\r\n"));
	assert(contains_bytes(out, out_len, "\r\n\r\nhi"));
}

/* ---------- Test 2: body-reading handler with prewarm ---------- */

static int echo_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t text_promise;
	jsval_promise_state_t state;
	jsval_t body_text;
	jsval_t response;
	jsval_t init;
	jsval_t promise;
	size_t input_len = 0;
	const char prefix[] = "echo: ";

	assert(jsval_request_text(region, request, &text_promise) == 0);
	assert(jsval_promise_state(region, text_promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(region, text_promise, &body_text) == 0);
	assert(jsval_string_copy_utf8(region, body_text, NULL, 0, &input_len)
			== 0);
	{
		size_t total_len = sizeof(prefix) - 1 + input_len;
		uint8_t buf[total_len ? total_len : 1];
		jsval_t body_string;

		memcpy(buf, prefix, sizeof(prefix) - 1);
		if (input_len > 0) {
			assert(jsval_string_copy_utf8(region, body_text,
					buf + sizeof(prefix) - 1, input_len, NULL) == 0);
		}
		assert(jsval_string_new_utf8(region, buf, total_len, &body_string)
				== 0);
		assert(jsval_object_new(region, 1, &init) == 0);
		assert(jsval_object_set_utf8(region, init,
				(const uint8_t *)"status", 6, jsval_number(200.0)) == 0);
		assert(jsval_response_new(region, body_string, 1, init, 1,
				&response) == 0);
	}
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_promise_resolve(region, promise, response) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_echo_handler_with_prewarm(void)
{
	static const uint8_t body[] = "hello world";
	uint8_t storage[131072];
	uint8_t out[512];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t response_promise;
	jsval_t response;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	size_t out_len = 0;
	faas_fake_body_t src = {
		.data = body, .total = sizeof(body) - 1, .cursor = 0,
		.chunk_size = 3, .close_calls = 0,
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST, &headers)
			== 0);
	assert(jsval_request_new_from_parts(&region, str(&region, "POST"),
			str(&region, "https://ex.com/"), headers, &faas_fake_vtable,
			&src, sizeof(body) - 1, &request) == 0);

	/* Prewarm: drain the streaming body into an eager body_buffer. */
	memset(&error, 0, sizeof(error));
	assert(jsval_request_prewarm_body(&region, request, &error) == 0);
	assert(src.close_calls == 1);

	/* Handler sees an eager body and resolves synchronously. */
	assert(echo_handler(&region, request, &response_promise) == 0);
	assert(faas_drain_until_settled(&region, response_promise, &error) == 0);
	assert(jsval_promise_state(&region, response_promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(&region, response_promise, &response) == 0);

	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) == 0);
	assert(contains_bytes(out, out_len, "HTTP/1.1 200 \r\n"));
	assert(contains_bytes(out, out_len, "Content-Length: 17\r\n"));
	assert(contains_bytes(out, out_len, "\r\n\r\necho: hello world"));
}

/* ---------- Test 3: Response.json handler ---------- */

static int json_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	static const char src_text[] = "{\"echo\":\"hi\"}";
	jsval_t data;
	jsval_t response;
	jsval_t promise;

	(void)request;
	assert(jsval_json_parse(region, (const uint8_t *)src_text,
			sizeof(src_text) - 1, 16, &data) == 0);
	assert(jsval_response_json(region, data, 0, jsval_undefined(),
			&response) == 0);
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_promise_resolve(region, promise, response) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_json_handler(void)
{
	uint8_t storage[65536];
	uint8_t out[512];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t response_promise;
	jsval_t response;
	jsmethod_error_t error;
	size_t out_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST, &headers)
			== 0);
	assert(jsval_request_new_from_parts(&region, jsval_undefined(),
			str(&region, "https://ex.com/"), headers, NULL, NULL,
			SIZE_MAX, &request) == 0);
	assert(json_handler(&region, request, &response_promise) == 0);
	memset(&error, 0, sizeof(error));
	assert(faas_drain_until_settled(&region, response_promise, &error) == 0);
	assert(jsval_promise_result(&region, response_promise, &response) == 0);
	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) == 0);
	assert(contains_bytes(out, out_len, "HTTP/1.1 200 \r\n"));
	assert(contains_bytes(out, out_len, "Content-Type: application/json"));
	assert(contains_bytes(out, out_len, "{\"echo\":\"hi\"}"));
}

/* ---------- Test 4: custom status and headers ---------- */

static int custom_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t init;
	jsval_t response;
	jsval_t headers;
	jsval_t promise;

	(void)request;
	assert(jsval_object_new(region, 2, &init) == 0);
	assert(jsval_object_set_utf8(region, init,
			(const uint8_t *)"status", 6, jsval_number(201.0)) == 0);
	assert(jsval_object_set_utf8(region, init,
			(const uint8_t *)"statusText", 10, str(region, "Created")) == 0);
	assert(jsval_response_new(region, str(region, "new"), 1, init, 1,
			&response) == 0);
	assert(jsval_response_headers(region, response, &headers) == 0);
	assert(jsval_headers_append(region, headers, str(region, "X-Foo"),
			str(region, "bar")) == 0);
	assert(jsval_headers_append(region, headers, str(region, "X-Baz"),
			str(region, "qux")) == 0);
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_promise_resolve(region, promise, response) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_custom_status_headers(void)
{
	uint8_t storage[65536];
	uint8_t out[512];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t response_promise;
	jsval_t response;
	jsmethod_error_t error;
	size_t out_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST, &headers)
			== 0);
	assert(jsval_request_new_from_parts(&region, jsval_undefined(),
			str(&region, "https://ex.com/"), headers, NULL, NULL,
			SIZE_MAX, &request) == 0);
	assert(custom_handler(&region, request, &response_promise) == 0);
	memset(&error, 0, sizeof(error));
	assert(faas_drain_until_settled(&region, response_promise, &error) == 0);
	assert(jsval_promise_result(&region, response_promise, &response) == 0);
	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) == 0);
	assert(contains_bytes(out, out_len, "HTTP/1.1 201 Created\r\n"));
	assert(contains_bytes(out, out_len, "X-Foo: bar\r\n"));
	assert(contains_bytes(out, out_len, "X-Baz: qux\r\n"));
	assert(contains_bytes(out, out_len, "\r\n\r\nnew"));
}

/* ---------- Test 5: Content-Length override ---------- */

static int cl_override_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t init;
	jsval_t response;
	jsval_t headers;
	jsval_t promise;

	(void)request;
	assert(jsval_object_new(region, 1, &init) == 0);
	assert(jsval_object_set_utf8(region, init,
			(const uint8_t *)"status", 6, jsval_number(200.0)) == 0);
	assert(jsval_response_new(region, str(region, "actual"), 1, init, 1,
			&response) == 0);
	assert(jsval_response_headers(region, response, &headers) == 0);
	/* A bogus Content-Length that the serializer should ignore. */
	assert(jsval_headers_append(region, headers, str(region, "Content-Length"),
			str(region, "999")) == 0);
	assert(jsval_headers_append(region, headers,
			str(region, "Transfer-Encoding"), str(region, "chunked")) == 0);
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_promise_resolve(region, promise, response) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_content_length_override(void)
{
	uint8_t storage[65536];
	uint8_t out[512];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t response_promise;
	jsval_t response;
	jsmethod_error_t error;
	size_t out_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST, &headers)
			== 0);
	assert(jsval_request_new_from_parts(&region, jsval_undefined(),
			str(&region, "https://ex.com/"), headers, NULL, NULL,
			SIZE_MAX, &request) == 0);
	assert(cl_override_handler(&region, request, &response_promise) == 0);
	memset(&error, 0, sizeof(error));
	assert(faas_drain_until_settled(&region, response_promise, &error) == 0);
	assert(jsval_promise_result(&region, response_promise, &response) == 0);
	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) == 0);
	/* Exactly one Content-Length header, with the real length (6). */
	assert(count_substrings(out, out_len, "Content-Length:") == 1);
	assert(contains_bytes(out, out_len, "Content-Length: 6\r\n"));
	assert(!contains_bytes(out, out_len, "Content-Length: 999"));
	assert(!contains_bytes(out, out_len, "Transfer-Encoding"));
	assert(contains_bytes(out, out_len, "\r\n\r\nactual"));
}

/* ---------- Test 6: Response.error() serialization rejection ---------- */

static void test_error_response_rejection(void)
{
	uint8_t storage[32768];
	uint8_t out[256];
	jsval_region_t region;
	jsval_t response;
	size_t out_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_response_error(&region, &response) == 0);
	errno = 0;
	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) < 0);
	assert(errno == EINVAL);
}

/* ---------- Test 7: ENOBUFS buffer-too-small ---------- */

static void test_buffer_too_small(void)
{
	uint8_t storage[32768];
	uint8_t out[8]; /* deliberately too small */
	jsval_region_t region;
	jsval_t init;
	jsval_t response;
	size_t out_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_object_new(&region, 1, &init) == 0);
	assert(jsval_object_set_utf8(&region, init,
			(const uint8_t *)"status", 6, jsval_number(200.0)) == 0);
	assert(jsval_response_new(&region, str(&region, "hello"), 1, init, 1,
			&response) == 0);
	errno = 0;
	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) < 0);
	assert(errno == ENOBUFS);
	assert(out_len > sizeof(out));
}

/* ---------- Test 8: EDEADLK on forgotten resolve ---------- */

static void test_deadlock_detection(void)
{
	uint8_t storage[32768];
	jsval_region_t region;
	jsval_t promise;
	jsmethod_error_t error;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_promise_new(&region, &promise) == 0);
	/* Don't resolve it and don't enqueue anything. */
	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(faas_drain_until_settled(&region, promise, &error) < 0);
	assert(errno == EDEADLK);
}

/* ---------- Test 9: streaming response body rejection ---------- */

static int streaming_response_handler(jsval_region_t *region,
		jsval_t request, jsval_t *response_promise_out,
		faas_fake_body_t *src)
{
	jsval_t headers;
	jsval_t response;
	jsval_t promise;

	(void)request;
	assert(jsval_headers_new(region, JSVAL_HEADERS_GUARD_RESPONSE, &headers)
			== 0);
	assert(jsval_response_new_from_parts(region, 200, jsval_undefined(),
			headers, &faas_fake_vtable, src, SIZE_MAX, &response) == 0);
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_promise_resolve(region, promise, response) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_streaming_response_rejection(void)
{
	static const uint8_t body[] = "streaming";
	uint8_t storage[65536];
	uint8_t out[512];
	jsval_region_t region;
	jsval_t request_headers;
	jsval_t request;
	jsval_t response_promise;
	jsval_t response;
	jsmethod_error_t error;
	size_t out_len = 0;
	faas_fake_body_t src = {
		.data = body, .total = sizeof(body) - 1, .cursor = 0,
		.chunk_size = 0, .close_calls = 0,
	};

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST,
			&request_headers) == 0);
	assert(jsval_request_new_from_parts(&region, jsval_undefined(),
			str(&region, "https://ex.com/"), request_headers, NULL, NULL,
			SIZE_MAX, &request) == 0);
	assert(streaming_response_handler(&region, request, &response_promise,
			&src) == 0);
	memset(&error, 0, sizeof(error));
	assert(faas_drain_until_settled(&region, response_promise, &error) == 0);
	assert(jsval_promise_result(&region, response_promise, &response) == 0);
	errno = 0;
	assert(faas_serialize_response(&region, response, out, sizeof(out),
			&out_len) < 0);
	assert(errno == ENOTSUP);
}

/* ---------- Driver ---------- */

int main(void)
{
	test_plain_handler();
	test_echo_handler_with_prewarm();
	test_json_handler();
	test_custom_status_headers();
	test_content_length_override();
	test_error_response_rejection();
	test_buffer_too_small();
	test_deadlock_detection();
	test_streaming_response_rejection();
	puts("test_faas: ok");
	return 0;
}

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
	jsval_t body_string;
	jsval_t response;
	jsval_t init;
	jsval_t promise;
	static const uint8_t prefix[] = "echo: ";

	assert(jsval_request_text(region, request, &text_promise) == 0);
	assert(jsval_promise_state(region, text_promise, &state) == 0);
	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_promise_result(region, text_promise, &body_text) == 0);
	assert(jsval_string_concat_utf8(region, prefix, sizeof(prefix) - 1,
			body_text, &body_string) == 0);
	assert(jsval_object_new(region, 1, &init) == 0);
	assert(jsval_object_set_utf8(region, init,
			(const uint8_t *)"status", 6, jsval_number(200.0)) == 0);
	assert(jsval_response_new(region, body_string, 1, init, 1,
			&response) == 0);
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

/* ---------- Test 10: rejecting handler through faas_run_and_extract ---------- */

static int rejecting_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t promise;
	jsval_t reason;
	jsval_t name_val;
	jsval_t msg_val;

	(void)request;
	assert(jsval_promise_new(region, &promise) == 0);
	assert(jsval_string_new_utf8(region, (const uint8_t *)"TypeError", 9,
			&name_val) == 0);
	assert(jsval_string_new_utf8(region,
			(const uint8_t *)"handler refused request", 23, &msg_val) == 0);
	assert(jsval_dom_exception_new(region, name_val, msg_val, &reason) == 0);
	assert(jsval_promise_reject(region, promise, reason) == 0);
	*response_promise_out = promise;
	return 0;
}

static void test_rejecting_handler_run_and_extract(void)
{
	uint8_t storage[65536];
	jsval_region_t region;
	jsval_t headers;
	jsval_t request;
	jsval_t reason_out;
	jsval_t name;
	jsmethod_error_t error;
	size_t name_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_REQUEST, &headers)
			== 0);
	assert(jsval_request_new_from_parts(&region, jsval_undefined(),
			str(&region, "https://ex.com/"), headers, NULL, NULL,
			SIZE_MAX, &request) == 0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(faas_run_and_extract(&region, rejecting_handler, request,
			&reason_out, &error) < 0);
	assert(errno == EIO);
	/* reason_out holds the rejection reason — inspect its name. */
	assert(jsval_dom_exception_name(&region, reason_out, &name) == 0);
	assert(jsval_string_copy_utf8(&region, name, NULL, 0, &name_len) == 0);
	assert(name_len == 9);
	{
		uint8_t buf[9];
		assert(jsval_string_copy_utf8(&region, name, buf, 9, NULL) == 0);
		assert(memcmp(buf, "TypeError", 9) == 0);
	}
}

/* ---------- Test 11: faas_build_request_from_parts + faas_run_and_extract happy path ---------- */

static void test_build_from_parts_and_run(void)
{
	uint8_t storage[131072];
	uint8_t out[1024];
	jsval_region_t region;
	jsval_t request_value;
	jsval_t response_value;
	jsmethod_error_t error;
	size_t out_len = 0;
	static const char body[] = "ping";
	static const uint8_t url[] = "/echo";
	faas_header_pair_t pairs[2] = {
		{ (const uint8_t *)"Content-Type", 12,
		  (const uint8_t *)"text/plain", 10 },
		{ (const uint8_t *)"X-Demo", 6,
		  (const uint8_t *)"ok", 2 },
	};

	jsval_region_init(&region, storage, sizeof(storage));

	assert(faas_build_request_from_parts(&region, "POST",
			url, sizeof(url) - 1, pairs, 2,
			(const uint8_t *)body, sizeof(body) - 1,
			&request_value) == 0);
	assert(request_value.kind == JSVAL_KIND_REQUEST);

	memset(&error, 0, sizeof(error));
	assert(faas_run_and_extract(&region, echo_handler, request_value,
			&response_value, &error) == 0);
	assert(response_value.kind == JSVAL_KIND_RESPONSE);

	assert(faas_serialize_response(&region, response_value, out, sizeof(out),
			&out_len) == 0);
	{
		/* Expected body "echo: ping", 10 bytes. */
		size_t i;
		int found_echo = 0;
		const char needle[] = "echo: ping";
		for (i = 0; i + sizeof(needle) - 1 <= out_len; i++) {
			if (memcmp(out + i, needle, sizeof(needle) - 1) == 0) {
				found_echo = 1;
				break;
			}
		}
		assert(found_echo);
	}
}

/* ---------- Mock fetch transport ----------
 *
 * An in-process jsval_fetch_transport_t implementation that simulates an
 * upstream HTTP server without touching sockets. Used to exercise the
 * transport seam + FETCH_REQUEST microtask end-to-end.
 *
 * The shared config is passed as the transport userdata. Each begin()
 * call allocates a per-in-flight state via libc malloc (embedder-owned,
 * off-region) and counts pending/completed polls.
 */

typedef struct mock_shared_s {
	int begin_count;
	int poll_count;
	int cancel_count;

	/* Config: shape of the response returned on READY. */
	uint16_t resp_status;
	const char *resp_body;
	size_t resp_body_len;
	const char *resp_header_name;
	const char *resp_header_value;

	/* How many times poll should return PENDING before returning READY. */
	int pending_polls_before_ready;

	/* 0 = READY path, 1 = ERROR path. */
	int error_mode;
	const char *error_name;
	const char *error_msg;
} mock_shared_t;

typedef struct mock_state_s {
	int pending_remaining;
} mock_state_t;

static int mock_transport_begin(void *userdata, jsval_region_t *region,
		jsval_t request_value, void **state_out)
{
	mock_shared_t *shared = (mock_shared_t *)userdata;
	mock_state_t *s;

	(void)region;
	(void)request_value;
	shared->begin_count++;
	s = (mock_state_t *)calloc(1, sizeof(*s));
	if (s == NULL) {
		errno = ENOMEM;
		return -1;
	}
	s->pending_remaining = shared->pending_polls_before_ready;
	*state_out = s;
	return 0;
}

static int mock_transport_poll(void *userdata, jsval_region_t *region,
		void *state, jsval_fetch_transport_status_t *status_out,
		jsval_t *response_out, jsval_t *reason_out)
{
	mock_shared_t *shared = (mock_shared_t *)userdata;
	mock_state_t *s = (mock_state_t *)state;
	jsval_t headers;
	jsval_t init;
	jsval_t response;
	jsval_t status_text;
	jsval_t body_str;

	shared->poll_count++;
	if (s->pending_remaining > 0) {
		s->pending_remaining--;
		*status_out = JSVAL_FETCH_TRANSPORT_STATUS_PENDING;
		return 0;
	}
	if (shared->error_mode) {
		jsval_t name_val;
		jsval_t msg_val;
		jsval_t reason;
		const char *name = shared->error_name != NULL
				? shared->error_name : "TypeError";
		const char *msg = shared->error_msg != NULL
				? shared->error_msg : "mock transport error";

		if (jsval_string_new_utf8(region, (const uint8_t *)name,
				strlen(name), &name_val) < 0) {
			return -1;
		}
		if (jsval_string_new_utf8(region, (const uint8_t *)msg,
				strlen(msg), &msg_val) < 0) {
			return -1;
		}
		if (jsval_dom_exception_new(region, name_val, msg_val, &reason) < 0) {
			return -1;
		}
		*status_out = JSVAL_FETCH_TRANSPORT_STATUS_ERROR;
		*reason_out = reason;
		free(s);
		return 0;
	}

	/* READY path: construct a Response. */
	if (jsval_headers_new(region, JSVAL_HEADERS_GUARD_RESPONSE, &headers) < 0) {
		return -1;
	}
	if (shared->resp_header_name != NULL) {
		jsval_t name_val;
		jsval_t value_val;
		if (jsval_string_new_utf8(region,
				(const uint8_t *)shared->resp_header_name,
				strlen(shared->resp_header_name), &name_val) < 0) {
			return -1;
		}
		if (jsval_string_new_utf8(region,
				(const uint8_t *)shared->resp_header_value,
				strlen(shared->resp_header_value), &value_val) < 0) {
			return -1;
		}
		if (jsval_headers_append(region, headers, name_val, value_val) < 0) {
			return -1;
		}
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)"OK", 2,
			&status_text) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)shared->resp_body,
			shared->resp_body_len, &body_str) < 0) {
		return -1;
	}
	if (jsval_object_new(region, 3, &init) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init, (const uint8_t *)"status", 6,
			jsval_number((double)shared->resp_status)) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init, (const uint8_t *)"statusText", 10,
			status_text) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init, (const uint8_t *)"headers", 7,
			headers) < 0) {
		return -1;
	}
	if (jsval_response_new(region, body_str, 1, init, 1, &response) < 0) {
		return -1;
	}

	*status_out = JSVAL_FETCH_TRANSPORT_STATUS_READY;
	*response_out = response;
	free(s);
	return 0;
}

static void mock_transport_cancel(void *userdata, void *state)
{
	mock_shared_t *shared = (mock_shared_t *)userdata;
	shared->cancel_count++;
	free(state);
}

static const jsval_fetch_transport_t mock_transport_vtable = {
	mock_transport_begin,
	mock_transport_poll,
	mock_transport_cancel,
};

/* Proxy handler: calls fetch() once and returns the resulting promise.
 * When the transport is registered this exercises the FETCH_REQUEST
 * microtask path; when no transport is registered the promise is
 * rejected synchronously with TypeError("network not implemented yet"). */
static int mock_proxy_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t url;
	jsval_t upstream_promise;

	(void)request;
	if (jsval_string_new_utf8(region,
			(const uint8_t *)"http://example.com/path", 23, &url) < 0) {
		return -1;
	}
	if (jsval_fetch(region, url, jsval_undefined(), 0,
			&upstream_promise) < 0) {
		return -1;
	}
	*response_promise_out = upstream_promise;
	return 0;
}

/* Handler that fires two fetches and returns the second promise. Both
 * FETCH_REQUEST microtasks must be pumped before the drain settles. */
static int mock_two_fetch_handler(jsval_region_t *region, jsval_t request,
		jsval_t *response_promise_out)
{
	jsval_t url_a;
	jsval_t url_b;
	jsval_t promise_a;
	jsval_t promise_b;

	(void)request;
	if (jsval_string_new_utf8(region, (const uint8_t *)"http://a/", 9,
			&url_a) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)"http://b/", 9,
			&url_b) < 0) {
		return -1;
	}
	if (jsval_fetch(region, url_a, jsval_undefined(), 0, &promise_a) < 0) {
		return -1;
	}
	if (jsval_fetch(region, url_b, jsval_undefined(), 0, &promise_b) < 0) {
		return -1;
	}
	(void)promise_a;
	*response_promise_out = promise_b;
	return 0;
}

/* ---------- Test 12: mock transport one-shot success ---------- */

static void test_mock_transport_one_shot(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t request;
	jsval_t response;
	jsmethod_error_t error;
	mock_shared_t shared;

	jsval_region_init(&region, storage, sizeof(storage));

	memset(&shared, 0, sizeof(shared));
	shared.resp_status = 200;
	shared.resp_body = "proxied body";
	shared.resp_body_len = strlen(shared.resp_body);
	shared.resp_header_name = "X-Mock";
	shared.resp_header_value = "1";
	shared.pending_polls_before_ready = 0;
	shared.error_mode = 0;

	jsval_region_set_fetch_transport(&region, &mock_transport_vtable, &shared);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/", 1, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	assert(faas_run_and_extract(&region, mock_proxy_handler, request,
			&response, &error) == 0);
	assert(response.kind == JSVAL_KIND_RESPONSE);
	assert(shared.begin_count == 1);
	assert(shared.poll_count == 1);
	assert(shared.cancel_count == 0);

	/* Round-trip the response through the serializer and check status +
	 * header + body. */
	{
		uint8_t out[1024];
		size_t out_len = 0;
		assert(faas_serialize_response(&region, response, out, sizeof(out),
				&out_len) == 0);
		assert(contains_bytes(out, out_len, "HTTP/1.1 200 "));
		assert(contains_bytes(out, out_len, "X-Mock: 1"));
		assert(contains_bytes(out, out_len, "proxied body"));
	}
}

/* ---------- Test 13: mock transport staged PENDING → READY ---------- */

static void test_mock_transport_staged_pending(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t request;
	jsval_t response;
	jsmethod_error_t error;
	mock_shared_t shared;

	jsval_region_init(&region, storage, sizeof(storage));

	memset(&shared, 0, sizeof(shared));
	shared.resp_status = 200;
	shared.resp_body = "staged";
	shared.resp_body_len = strlen(shared.resp_body);
	shared.pending_polls_before_ready = 3;

	jsval_region_set_fetch_transport(&region, &mock_transport_vtable, &shared);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/", 1, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	assert(faas_run_and_extract(&region, mock_proxy_handler, request,
			&response, &error) == 0);
	assert(response.kind == JSVAL_KIND_RESPONSE);
	assert(shared.begin_count == 1);
	/* 3 PENDING + 1 READY = 4 poll calls. */
	assert(shared.poll_count == 4);
}

/* ---------- Test 14: mock transport ERROR rejects the handler promise ---------- */

static void test_mock_transport_error(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t request;
	jsval_t reason_out;
	jsval_t name_value;
	jsmethod_error_t error;
	mock_shared_t shared;
	size_t name_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));

	memset(&shared, 0, sizeof(shared));
	shared.error_mode = 1;
	shared.error_name = "TypeError";
	shared.error_msg = "mock upstream failed";

	jsval_region_set_fetch_transport(&region, &mock_transport_vtable, &shared);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/", 1, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(faas_run_and_extract(&region, mock_proxy_handler, request,
			&reason_out, &error) < 0);
	assert(errno == EIO);
	assert(shared.begin_count == 1);
	assert(shared.poll_count == 1);

	/* reason_out holds the rejection reason — a DOMException. */
	assert(jsval_dom_exception_name(&region, reason_out, &name_value) == 0);
	assert(jsval_string_copy_utf8(&region, name_value, NULL, 0, &name_len)
			== 0);
	assert(name_len == 9);
	{
		uint8_t buf[9];
		assert(jsval_string_copy_utf8(&region, name_value, buf, 9, NULL) == 0);
		assert(memcmp(buf, "TypeError", 9) == 0);
	}
}

/* ---------- Test 15: no transport registered → preserve reject-stub ---------- */

static void test_no_transport_rejects(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t request;
	jsval_t reason_out;
	jsval_t message_value;
	jsmethod_error_t error;
	size_t msg_len = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	/* Deliberately do not register a transport. */

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/", 1, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(faas_run_and_extract(&region, mock_proxy_handler, request,
			&reason_out, &error) < 0);
	assert(errno == EIO);

	/* The DOMException message should still be "network not implemented
	 * yet" to preserve the pre-seam contract for embedders without a
	 * transport. */
	assert(jsval_dom_exception_message(&region, reason_out, &message_value)
			== 0);
	assert(jsval_string_copy_utf8(&region, message_value, NULL, 0, &msg_len)
			== 0);
	{
		uint8_t buf[64];
		assert(msg_len < sizeof(buf));
		assert(jsval_string_copy_utf8(&region, message_value, buf, msg_len,
				NULL) == 0);
		assert(memcmp(buf, "network not implemented yet", msg_len) == 0);
	}
}

/* ---------- Test 16: two concurrent fetches → both microtasks pumped ---------- */

/* Declared in example/wintertc_proxy_handler.c. The Makefile links that
 * file into the test_faas binary so the hand-lowered proxy handler can
 * be exercised against the mock transport end-to-end. */
int wintertc_proxy_fetch_handler(jsval_region_t *region,
		jsval_t request_value, jsval_t *response_promise_out);

/* ---------- Test 17: hand-lowered proxy handler, success ---------- */

static void test_wintertc_proxy_handler_success(void)
{
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t request;
	jsval_t response;
	jsmethod_error_t error;
	mock_shared_t shared;

	jsval_region_init(&region, storage, sizeof(storage));

	memset(&shared, 0, sizeof(shared));
	shared.resp_status = 200;
	shared.resp_body = "greetings from upstream";
	shared.resp_body_len = strlen(shared.resp_body);
	shared.resp_header_name = "X-Upstream";
	shared.resp_header_value = "1";

	jsval_region_set_fetch_transport(&region, &mock_transport_vtable, &shared);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/echo", 5, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	assert(faas_run_and_extract(&region, wintertc_proxy_fetch_handler,
			request, &response, &error) == 0);
	assert(response.kind == JSVAL_KIND_RESPONSE);
	assert(shared.begin_count == 1);
	assert(shared.poll_count == 1);

	/* Serialize and check that status + forwarded header + body are all
	 * present — i.e. the handler proxied the upstream Response end to
	 * end. */
	{
		uint8_t out[1024];
		size_t out_len = 0;
		assert(faas_serialize_response(&region, response, out, sizeof(out),
				&out_len) == 0);
		assert(contains_bytes(out, out_len, "HTTP/1.1 200 "));
		assert(contains_bytes(out, out_len, "X-Upstream: 1"));
		assert(contains_bytes(out, out_len, "greetings from upstream"));
	}
}

/* ---------- Test 18: hand-lowered proxy handler, upstream error forwarding ---------- */

static void test_wintertc_proxy_handler_upstream_error(void)
{
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t request;
	jsval_t reason_out;
	jsmethod_error_t error;
	mock_shared_t shared;

	jsval_region_init(&region, storage, sizeof(storage));

	memset(&shared, 0, sizeof(shared));
	shared.error_mode = 1;
	shared.error_name = "TypeError";
	shared.error_msg = "upstream connect failed";

	jsval_region_set_fetch_transport(&region, &mock_transport_vtable, &shared);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/", 1, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	errno = 0;
	assert(faas_run_and_extract(&region, wintertc_proxy_fetch_handler,
			request, &reason_out, &error) < 0);
	assert(errno == EIO);
	/* The rejection should have propagated through jsval_promise_then
	 * with no explicit on_rejected handler — Promises/A+ forwarding. */
	{
		jsval_t name_value;
		size_t name_len = 0;
		uint8_t buf[16];
		assert(jsval_dom_exception_name(&region, reason_out, &name_value)
				== 0);
		assert(jsval_string_copy_utf8(&region, name_value, NULL, 0,
				&name_len) == 0);
		assert(name_len == 9);
		assert(jsval_string_copy_utf8(&region, name_value, buf, 9, NULL)
				== 0);
		assert(memcmp(buf, "TypeError", 9) == 0);
	}
}

/* ---------- Test 19: fetch waitlist mode, simulates coroutine driver ---------- */

/*
 * Simulates the mnvkd vk_jsmx_fetch_driver pattern: the handler calls
 * fetch() which parks a (Request, Promise) entry on the region's
 * waitlist; the test loop then plays the role of the coroutine driver,
 * draining microtasks, popping the waitlist entry, synthesizing a
 * Response in lieu of a real HTTP round trip, resolving the waitlist
 * promise, and looping until the handler's output Promise settles.
 */
static void test_fetch_waitlist_drives_proxy_handler(void)
{
	uint8_t storage[262144];
	jsval_region_t region;
	jsval_t request;
	jsval_t response_promise;
	jsval_t fetch_req;
	jsval_t fetch_prom;
	jsval_t response_value;
	jsval_promise_state_t state;
	jsmethod_error_t error;
	int drain_iterations = 0;

	jsval_region_init(&region, storage, sizeof(storage));
	jsval_region_set_fetch_waitlist_mode(&region, 1);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/echo", 5, NULL, 0, NULL, 0, &request) == 0);

	/* Invoke the handler directly so we can own the drain loop. */
	assert(wintertc_proxy_fetch_handler(&region, request, &response_promise)
			== 0);

	memset(&error, 0, sizeof(error));
	for (;;) {
		drain_iterations++;
		assert(drain_iterations < 32);

		assert(jsval_microtask_drain(&region, &error) == 0);
		assert(jsval_promise_state(&region, response_promise, &state) == 0);
		if (state != JSVAL_PROMISE_STATE_PENDING) {
			break;
		}
		assert(jsval_fetch_waitlist_size(&region) > 0);

		/* Pop one parked fetch and synthesize a Response for it. */
		assert(jsval_fetch_waitlist_pop(&region, &fetch_req, &fetch_prom)
				== 0);
		assert(fetch_req.kind == JSVAL_KIND_REQUEST);
		assert(fetch_prom.kind == JSVAL_KIND_PROMISE);

		/* Sanity check: the Request URL should have been built as
		 * "http://example.com" + "/echo" by the proxy handler. */
		{
			jsval_t url_value;
			size_t url_len = 0;
			uint8_t url_buf[64];
			assert(jsval_request_url(&region, fetch_req, &url_value) == 0);
			assert(jsval_string_copy_utf8(&region, url_value, NULL, 0,
					&url_len) == 0);
			assert(url_len < sizeof(url_buf));
			assert(jsval_string_copy_utf8(&region, url_value, url_buf,
					url_len, NULL) == 0);
			assert(memcmp(url_buf, "http://example.com/echo", 23) == 0);
			assert(url_len == 23);
		}

		/* Build the Response the way a real coroutine driver would. */
		{
			jsval_t headers;
			jsval_t name_val;
			jsval_t value_val;
			jsval_t status_text;
			jsval_t body_str;
			jsval_t init;
			jsval_t response;

			assert(jsval_headers_new(&region, JSVAL_HEADERS_GUARD_RESPONSE,
					&headers) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"X-Driver", 8, &name_val) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"waitlist", 8, &value_val) == 0);
			assert(jsval_headers_append(&region, headers, name_val,
					value_val) == 0);
			assert(jsval_string_new_utf8(&region, (const uint8_t *)"OK",
					2, &status_text) == 0);
			assert(jsval_string_new_utf8(&region,
					(const uint8_t *)"driven by waitlist", 18,
					&body_str) == 0);
			assert(jsval_object_new(&region, 3, &init) == 0);
			assert(jsval_object_set_utf8(&region, init,
					(const uint8_t *)"status", 6,
					jsval_number(200.0)) == 0);
			assert(jsval_object_set_utf8(&region, init,
					(const uint8_t *)"statusText", 10, status_text)
					== 0);
			assert(jsval_object_set_utf8(&region, init,
					(const uint8_t *)"headers", 7, headers) == 0);
			assert(jsval_response_new(&region, body_str, 1, init, 1,
					&response) == 0);
			assert(jsval_promise_resolve(&region, fetch_prom, response)
					== 0);
		}
	}

	assert(state == JSVAL_PROMISE_STATE_FULFILLED);
	assert(jsval_fetch_waitlist_size(&region) == 0);
	assert(jsval_promise_result(&region, response_promise, &response_value)
			== 0);
	assert(response_value.kind == JSVAL_KIND_RESPONSE);

	/* Serialize the final Response — the proxy handler should have
	 * forwarded the upstream status, header, and body. */
	{
		uint8_t out[1024];
		size_t out_len = 0;
		assert(faas_serialize_response(&region, response_value, out,
				sizeof(out), &out_len) == 0);
		assert(contains_bytes(out, out_len, "HTTP/1.1 200 "));
		assert(contains_bytes(out, out_len, "X-Driver: waitlist"));
		assert(contains_bytes(out, out_len, "driven by waitlist"));
	}
}

static void test_mock_transport_two_concurrent(void)
{
	uint8_t storage[131072];
	jsval_region_t region;
	jsval_t request;
	jsval_t response;
	jsmethod_error_t error;
	mock_shared_t shared;

	jsval_region_init(&region, storage, sizeof(storage));

	memset(&shared, 0, sizeof(shared));
	shared.resp_status = 200;
	shared.resp_body = "twin";
	shared.resp_body_len = strlen(shared.resp_body);
	shared.pending_polls_before_ready = 2;

	jsval_region_set_fetch_transport(&region, &mock_transport_vtable, &shared);

	assert(faas_build_request_from_parts(&region, "GET",
			(const uint8_t *)"/", 1, NULL, 0, NULL, 0, &request) == 0);

	memset(&error, 0, sizeof(error));
	assert(faas_run_and_extract(&region, mock_two_fetch_handler, request,
			&response, &error) == 0);
	assert(response.kind == JSVAL_KIND_RESPONSE);
	assert(shared.begin_count == 2);
	/* Each of 2 fetches: 2 PENDING + 1 READY = 3 polls, * 2 = 6 total. */
	assert(shared.poll_count == 6);
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
	test_rejecting_handler_run_and_extract();
	test_build_from_parts_and_run();
	test_mock_transport_one_shot();
	test_mock_transport_staged_pending();
	test_mock_transport_error();
	test_no_transport_rejects();
	test_mock_transport_two_concurrent();
	test_wintertc_proxy_handler_success();
	test_wintertc_proxy_handler_upstream_error();
	test_fetch_waitlist_drives_proxy_handler();
	puts("test_faas: ok");
	return 0;
}

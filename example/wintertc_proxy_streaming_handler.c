#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "jsmethod.h"
#include "jsval.h"

/*
 * Source JS: example/wintertc_proxy_streaming.js
 *
 *   const JSMX_UPSTREAM_BASE = "http://example.com";
 *   export default {
 *     async fetch(request) {
 *       const upstream = await fetch(JSMX_UPSTREAM_BASE + request.url);
 *       const reader = upstream.body.getReader();
 *       const chunks = [];
 *       let total = 0;
 *       while (true) {
 *         const { value, done } = await reader.read();
 *         if (done) break;
 *         chunks.push(value);
 *         total += value.length;
 *       }
 *       const out = new Uint8Array(total);
 *       let offset = 0;
 *       for (const chunk of chunks) {
 *         out.set(chunk, offset);
 *         offset += chunk.length;
 *       }
 *       return new Response(out, {
 *         status: upstream.status,
 *         statusText: upstream.statusText,
 *         headers: upstream.headers,
 *       });
 *     },
 *   };
 *
 * Lowering class: production_slow_path. The outer `await fetch(...)`
 * is one CPS continuation (same as wintertc_proxy_handler.c). The
 * inner `await reader.read()` loop is a SECOND CPS chain — one
 * continuation (`on_chunk`) registered repeatedly via
 * jsval_promise_then on each read Promise. The continuation
 * accumulates bytes until {done: true}, then builds the final
 * Response.
 *
 * State threading limitation: jsval_promise_then has no closure
 * userdata, and jsval_native_function_fn receives only argv. To
 * survive across iterations the loop's state lives in a single
 * static struct streaming_chain_state. This is sufficient for the
 * single-fetch FaaS demo (one in-flight chain per process); a
 * future jsmx promise-then closure API would let multiple chains
 * coexist. The limitation is documented inline; a streaming
 * handler that needs concurrent reads should not be transpiled
 * from this template until that API lands.
 *
 * Host assumptions match wintertc_proxy_handler.c — embedder
 * registers a fetch transport (ideally one driving the streaming
 * driver via spec.streaming = 1, e.g. vk_fetch_url_launch_streaming
 * in mnvkd) before invoking the handler.
 */

#ifndef WINTERTC_PROXY_UPSTREAM_BASE
#define WINTERTC_PROXY_UPSTREAM_BASE "http://example.com"
#endif
#define WINTERTC_PROXY_UPSTREAM_BASE_LEN \
	(sizeof(WINTERTC_PROXY_UPSTREAM_BASE) - 1)

#ifndef WINTERTC_PROXY_STREAMING_BODY_MAX
#define WINTERTC_PROXY_STREAMING_BODY_MAX (256u * 1024u)
#endif
#ifndef WINTERTC_PROXY_STREAMING_STATUS_TEXT_MAX
#define WINTERTC_PROXY_STREAMING_STATUS_TEXT_MAX 64u
#endif

/*
 * Per-handler-process state. Initialized at the top of
 * on_upstream_fulfilled; consumed iteratively by on_chunk; cleared
 * when the chain produces the final Response. Single-chain only.
 */
struct streaming_chain_state {
	jsval_t reader;
	uint8_t accum[WINTERTC_PROXY_STREAMING_BODY_MAX];
	size_t accum_len;
	uint16_t status;
	jsval_t status_text_value;
	jsval_t headers_value;
	int active;
};

static struct streaming_chain_state g_state;

static int wintertc_proxy_streaming_build_final_response(
		jsval_region_t *region, jsval_t *response_out)
{
	jsval_t out_buffer;
	uint8_t *out_bytes = NULL;
	size_t out_cap = 0;
	jsval_t init_value;

	if (jsval_array_buffer_new(region,
			g_state.accum_len == 0 ? 1 : g_state.accum_len,
			&out_buffer) < 0) {
		return -1;
	}
	if (g_state.accum_len > 0) {
		if (jsval_array_buffer_bytes_mut(region, out_buffer,
				&out_bytes, &out_cap) < 0) {
			return -1;
		}
		if (out_cap < g_state.accum_len) {
			errno = EIO;
			return -1;
		}
		memcpy(out_bytes, g_state.accum, g_state.accum_len);
	}

	if (jsval_object_new(region, 3, &init_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"status", 6,
			jsval_number((double)g_state.status)) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"statusText", 10,
			g_state.status_text_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"headers", 7,
			g_state.headers_value) < 0) {
		return -1;
	}
	return jsval_response_new(region, out_buffer, 1, init_value, 1,
			response_out);
}

/*
 * Per-chunk continuation. argv[0] is the { value, done } object
 * from the most-recent reader.read() Promise. On `done == true`
 * we build and return the final Response. Otherwise we accumulate
 * the chunk's bytes and re-register self on the NEXT reader.read()
 * Promise via jsval_promise_then; the chained Promise is returned
 * via *result_ptr so the microtask drain wires it into the outer
 * downstream Promise.
 */
static int wintertc_proxy_streaming_on_chunk(jsval_region_t *region,
		size_t argc, const jsval_t *argv, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	jsval_t chunk;
	jsval_t done_value;
	jsval_t value_value;
	jsval_typed_array_kind_t kind;
	jsval_t backing;
	uint8_t *bytes = NULL;
	size_t backing_len = 0;
	size_t chunk_len;
	jsval_t next_promise;
	jsval_t continuation_fn;
	jsval_t downstream_promise;
	jsval_t final_response;

	(void)error;

	if (argc < 1 || result_ptr == NULL || !g_state.active) {
		errno = EINVAL;
		return -1;
	}
	chunk = argv[0];

	if (jsval_object_get_utf8(region, chunk,
			(const uint8_t *)"done", 4, &done_value) < 0) {
		return -1;
	}
	if (done_value.kind == JSVAL_KIND_BOOL && done_value.as.boolean) {
		if (wintertc_proxy_streaming_build_final_response(region,
				&final_response) < 0) {
			return -1;
		}
		g_state.active = 0;
		*result_ptr = final_response;
		return 0;
	}

	if (jsval_object_get_utf8(region, chunk,
			(const uint8_t *)"value", 5, &value_value) < 0) {
		return -1;
	}
	if (value_value.kind != JSVAL_KIND_TYPED_ARRAY) {
		errno = EIO;
		return -1;
	}
	if (jsval_typed_array_kind(region, value_value, &kind) < 0
			|| kind != JSVAL_TYPED_ARRAY_UINT8) {
		errno = EIO;
		return -1;
	}
	chunk_len = jsval_typed_array_length(region, value_value);
	if (chunk_len > 0) {
		if (jsval_typed_array_buffer(region, value_value,
				&backing) < 0) {
			return -1;
		}
		if (jsval_array_buffer_bytes_mut(region, backing, &bytes,
				&backing_len) < 0 || backing_len < chunk_len) {
			errno = EIO;
			return -1;
		}
		if (g_state.accum_len + chunk_len > sizeof(g_state.accum)) {
			errno = EMSGSIZE;
			return -1;
		}
		memcpy(g_state.accum + g_state.accum_len, bytes, chunk_len);
		g_state.accum_len += chunk_len;
	}

	if (jsval_readable_stream_reader_read(region, g_state.reader,
			&next_promise) < 0) {
		return -1;
	}
	if (jsval_function_new(region, wintertc_proxy_streaming_on_chunk,
			1, 0, jsval_undefined(), &continuation_fn) < 0) {
		return -1;
	}
	if (jsval_promise_then(region, next_promise, continuation_fn,
			jsval_undefined(), &downstream_promise) < 0) {
		return -1;
	}
	*result_ptr = downstream_promise;
	return 0;
}

/*
 * CPS continuation invoked when the upstream fetch Promise fulfills.
 * argv[0] is the upstream Response. Captures the response metadata
 * into g_state, opens response.body, acquires the reader, and kicks
 * off the read loop by registering on_chunk on the first
 * reader.read() Promise.
 */
static int wintertc_proxy_streaming_on_upstream_fulfilled(
		jsval_region_t *region, size_t argc, const jsval_t *argv,
		jsval_t *result_ptr, jsmethod_error_t *error)
{
	jsval_t upstream;
	jsval_t stream;
	jsval_t reader;
	jsval_t first_promise;
	jsval_t continuation_fn;
	jsval_t downstream_promise;
	uint32_t status_u32 = 0;

	(void)error;

	if (argc < 1 || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	upstream = argv[0];
	if (upstream.kind != JSVAL_KIND_RESPONSE) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_response_status(region, upstream, &status_u32) < 0) {
		return -1;
	}
	g_state.status = (uint16_t)status_u32;
	if (jsval_response_status_text(region, upstream,
			&g_state.status_text_value) < 0) {
		return -1;
	}
	if (jsval_response_headers(region, upstream,
			&g_state.headers_value) < 0) {
		return -1;
	}

	if (jsval_response_body(region, upstream, &stream) < 0
			|| stream.kind != JSVAL_KIND_READABLE_STREAM) {
		errno = EIO;
		return -1;
	}
	if (jsval_readable_stream_get_reader(region, stream, &reader) < 0) {
		return -1;
	}

	g_state.reader = reader;
	g_state.accum_len = 0;
	g_state.active = 1;

	if (jsval_readable_stream_reader_read(region, reader,
			&first_promise) < 0) {
		return -1;
	}
	if (jsval_function_new(region, wintertc_proxy_streaming_on_chunk,
			1, 0, jsval_undefined(), &continuation_fn) < 0) {
		return -1;
	}
	if (jsval_promise_then(region, first_promise, continuation_fn,
			jsval_undefined(), &downstream_promise) < 0) {
		return -1;
	}
	*result_ptr = downstream_promise;
	return 0;
}

/*
 * Entrypoint matching faas_fetch_handler_fn. Same outer shape as
 * wintertc_proxy_fetch_handler — issue the upstream fetch, register
 * a CPS continuation on the upstream Promise, return the downstream
 * Promise. The streaming-specific logic lives in the continuation
 * chain (on_upstream_fulfilled → on_chunk*).
 */
int wintertc_proxy_streaming_fetch_handler(jsval_region_t *region,
		jsval_t request_value, jsval_t *response_promise_out)
{
	jsval_t request_url;
	jsval_t base_value;
	jsval_t upstream_url;
	jsval_t concat_args[1];
	jsval_t upstream_promise;
	jsval_t continuation_fn;
	jsval_t downstream_promise;
	jsmethod_error_t error;

	if (response_promise_out == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_request_url(region, request_value, &request_url) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region,
			(const uint8_t *)WINTERTC_PROXY_UPSTREAM_BASE,
			WINTERTC_PROXY_UPSTREAM_BASE_LEN, &base_value) < 0) {
		return -1;
	}
	concat_args[0] = request_url;
	memset(&error, 0, sizeof(error));
	if (jsval_method_string_concat(region, base_value, 1, concat_args,
			&upstream_url, &error) < 0) {
		return -1;
	}
	if (jsval_fetch(region, upstream_url, jsval_undefined(), 0,
			&upstream_promise) < 0) {
		return -1;
	}

	if (jsval_function_new(region,
			wintertc_proxy_streaming_on_upstream_fulfilled, 1, 0,
			jsval_undefined(), &continuation_fn) < 0) {
		return -1;
	}
	if (jsval_promise_then(region, upstream_promise, continuation_fn,
			jsval_undefined(), &downstream_promise) < 0) {
		return -1;
	}

	*response_promise_out = downstream_promise;
	return 0;
}

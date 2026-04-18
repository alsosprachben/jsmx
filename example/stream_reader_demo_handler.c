#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "jsmethod.h"
#include "jsval.h"

/*
 * Source JS: example/stream_reader_demo.js
 *
 *   const STREAM_DEMO_PAYLOAD = "hello from a readable stream";
 *   export default {
 *     async fetch(request) {
 *       const source = new Response(STREAM_DEMO_PAYLOAD);
 *       const reader = source.body.getReader();
 *
 *       const chunks = [];
 *       let total = 0;
 *       while (true) {
 *         const { value, done } = await reader.read();
 *         if (done) break;
 *         chunks.push(value);
 *         total += value.length;
 *       }
 *
 *       const out = new Uint8Array(total);
 *       let offset = 0;
 *       for (const chunk of chunks) {
 *         out.set(chunk, offset);
 *         offset += chunk.length;
 *       }
 *
 *       return new Response(out, {
 *         status: 200,
 *         headers: { "content-type": "text/plain" },
 *       });
 *     },
 *   };
 *
 * Behavior: the handler ignores the inbound request, constructs an
 * in-memory Response from STREAM_DEMO_PAYLOAD, drains its body
 * through the Phase-1 ReadableStream API, and returns the
 * concatenated bytes as a new Response.
 *
 * Lowering class: production_flattened.
 *
 *   - Every reader.read() on an in-memory Response body fulfills
 *     synchronously, so the JS while-loop lowers to a plain C for-
 *     loop with the sync-extract idiom (same reasoning as the
 *     Body Consumption Shortcut recipe applied per chunk).
 *   - The handler therefore performs the entire body drain inside
 *     its own stack frame and returns an already-fulfilled Promise
 *     wrapping the output Response. No CPS continuation is
 *     allocated; a future streaming-body source (e.g. the mnvkd
 *     vk_socket adapter) would flip this to production_slow_path
 *     and grow one continuation per read.
 *
 * Host assumptions:
 *   - The FaaS bridge invokes this handler inside a region it owns
 *     (same contract as example/wintertc_proxy_handler.c). The
 *     handler does not require a registered fetch transport.
 *   - The accumulator is sized for the baked-in payload; a larger
 *     source would require a dynamic buffer.
 */

#define STREAM_READER_DEMO_PAYLOAD "hello from a readable stream"
#define STREAM_READER_DEMO_PAYLOAD_LEN \
	(sizeof(STREAM_READER_DEMO_PAYLOAD) - 1)

static int stream_reader_demo_build_response(jsval_region_t *region,
		jsval_t *response_out)
{
	jsval_t payload_value;
	jsval_t source_response;
	jsval_t stream;
	jsval_t reader;
	uint8_t accumulator[128];
	size_t accumulator_len = 0;
	jsval_t out_buffer;
	uint8_t *out_bytes = NULL;
	size_t out_cap = 0;
	jsval_t headers_value;
	jsval_t header_value_value;
	jsval_t init_value;

	/* const source = new Response(STREAM_DEMO_PAYLOAD); */
	if (jsval_string_new_utf8(region,
			(const uint8_t *)STREAM_READER_DEMO_PAYLOAD,
			STREAM_READER_DEMO_PAYLOAD_LEN, &payload_value) < 0) {
		return -1;
	}
	if (jsval_response_new(region, payload_value, 1, jsval_undefined(), 0,
			&source_response) < 0) {
		return -1;
	}

	/* const reader = source.body.getReader(); */
	if (jsval_response_body(region, source_response, &stream) < 0) {
		return -1;
	}
	if (stream.kind != JSVAL_KIND_READABLE_STREAM) {
		errno = EIO;
		return -1;
	}
	if (jsval_readable_stream_get_reader(region, stream, &reader) < 0) {
		return -1;
	}

	/* while (true) { const { value, done } = await reader.read(); ... } */
	for (;;) {
		jsval_t read_promise;
		jsval_promise_state_t state;
		jsval_t chunk_obj;
		jsval_t done_value;
		jsval_t chunk_value;
		jsval_typed_array_kind_t kind;
		jsval_t backing;
		uint8_t *chunk_bytes = NULL;
		size_t backing_len = 0;
		size_t chunk_len;

		if (jsval_readable_stream_reader_read(region, reader,
				&read_promise) < 0) {
			return -1;
		}
		if (jsval_promise_state(region, read_promise, &state) < 0) {
			return -1;
		}
		if (state != JSVAL_PROMISE_STATE_FULFILLED) {
			/* Streaming body path — not reachable with the in-memory
			 * source above; would become a CPS continuation. */
			errno = EIO;
			return -1;
		}
		if (jsval_promise_result(region, read_promise, &chunk_obj) < 0) {
			return -1;
		}
		if (jsval_object_get_utf8(region, chunk_obj,
				(const uint8_t *)"done", 4, &done_value) < 0) {
			return -1;
		}
		if (done_value.kind == JSVAL_KIND_BOOL
				&& done_value.as.boolean) {
			break;
		}
		if (jsval_object_get_utf8(region, chunk_obj,
				(const uint8_t *)"value", 5, &chunk_value) < 0) {
			return -1;
		}
		if (chunk_value.kind != JSVAL_KIND_TYPED_ARRAY) {
			errno = EIO;
			return -1;
		}
		if (jsval_typed_array_kind(region, chunk_value, &kind) < 0
				|| kind != JSVAL_TYPED_ARRAY_UINT8) {
			errno = EIO;
			return -1;
		}
		chunk_len = jsval_typed_array_length(region, chunk_value);
		if (jsval_typed_array_buffer(region, chunk_value, &backing) < 0) {
			return -1;
		}
		if (jsval_array_buffer_bytes_mut(region, backing, &chunk_bytes,
				&backing_len) < 0 || backing_len < chunk_len) {
			errno = EIO;
			return -1;
		}
		if (chunk_len > sizeof(accumulator) - accumulator_len) {
			errno = EMSGSIZE;
			return -1;
		}
		if (chunk_len > 0) {
			memcpy(accumulator + accumulator_len, chunk_bytes, chunk_len);
			accumulator_len += chunk_len;
		}
	}

	/* return new Response(out, { status: 200, headers: {...} }); */
	if (jsval_array_buffer_new(region, accumulator_len, &out_buffer) < 0) {
		return -1;
	}
	if (jsval_array_buffer_bytes_mut(region, out_buffer, &out_bytes,
			&out_cap) < 0 || out_cap < accumulator_len) {
		errno = EIO;
		return -1;
	}
	if (accumulator_len > 0) {
		memcpy(out_bytes, accumulator, accumulator_len);
	}

	if (jsval_headers_new(region, JSVAL_HEADERS_GUARD_RESPONSE,
			&headers_value) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)"text/plain", 10,
			&header_value_value) < 0) {
		return -1;
	}
	{
		jsval_t name_value;
		if (jsval_string_new_utf8(region,
				(const uint8_t *)"content-type", 12, &name_value) < 0) {
			return -1;
		}
		if (jsval_headers_append(region, headers_value, name_value,
				header_value_value) < 0) {
			return -1;
		}
	}

	if (jsval_object_new(region, 2, &init_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"status", 6, jsval_number(200.0)) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"headers", 7, headers_value) < 0) {
		return -1;
	}

	return jsval_response_new(region, out_buffer, 1, init_value, 1,
			response_out);
}

/*
 * Entrypoint matching faas_fetch_handler_fn. The body is built
 * synchronously and handed back through an already-fulfilled
 * downstream Promise.
 */
int stream_reader_demo_fetch_handler(jsval_region_t *region,
		jsval_t request_value, jsval_t *response_promise_out)
{
	jsval_t response_value;
	jsval_t downstream_promise;

	(void)request_value;

	if (response_promise_out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (stream_reader_demo_build_response(region, &response_value) < 0) {
		return -1;
	}
	if (jsval_promise_new(region, &downstream_promise) < 0) {
		return -1;
	}
	if (jsval_promise_resolve(region, downstream_promise, response_value)
			< 0) {
		return -1;
	}
	*response_promise_out = downstream_promise;
	return 0;
}

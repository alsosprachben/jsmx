#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "jsmethod.h"
#include "jsval.h"

/*
 * Source JS: example/wintertc_proxy_concurrent.js
 *
 *   const JSMX_UPSTREAM_BASE = "http://example.com";
 *   export default {
 *     async fetch(request) {
 *       const [a, b] = await Promise.all([
 *         fetch(JSMX_UPSTREAM_BASE + "/part-a.txt"),
 *         fetch(JSMX_UPSTREAM_BASE + "/part-b.txt"),
 *       ]);
 *       const [bodyA, bodyB] = await Promise.all([
 *         a.arrayBuffer(),
 *         b.arrayBuffer(),
 *       ]);
 *       const out = new Uint8Array(bodyA.byteLength + bodyB.byteLength);
 *       out.set(new Uint8Array(bodyA), 0);
 *       out.set(new Uint8Array(bodyB), bodyA.byteLength);
 *       return new Response(out.buffer, { status: 200 });
 *     },
 *   };
 *
 * Exercise handler for the vk_fetch_url_launch / vk_fetch_url_gather
 * concurrent-dispatch path in vk_test_faas_demo.c. Issues two
 * parallel upstream fetches, joins via jsval_promise_all, and
 * returns a single Response whose body is the byte concatenation
 * of both upstream bodies.
 *
 * Lowering: the outer Promise.all is the new jsval_promise_all
 * primitive — no per-fetch .then callbacks. The second "wait both
 * body arrayBuffers" Promise.all is collapsed inline because the
 * demo transport materialises Response bodies eagerly, so every
 * `response.arrayBuffer()` Promise is already fulfilled by the
 * time the continuation runs.
 */

#ifndef WINTERTC_PROXY_UPSTREAM_BASE
#define WINTERTC_PROXY_UPSTREAM_BASE "http://example.com"
#endif
#define WINTERTC_PROXY_UPSTREAM_BASE_LEN \
	(sizeof(WINTERTC_PROXY_UPSTREAM_BASE) - 1)

static const uint8_t wintertc_proxy_concurrent_path_a[] = "/part-a.txt";
static const uint8_t wintertc_proxy_concurrent_path_b[] = "/part-b.txt";

/*
 * CPS continuation invoked when Promise.all fulfills. argv[0] is a
 * 2-element array of upstream Responses, in dispatch order. Reads
 * both bodies (eagerly fulfilled for in-memory responses), builds a
 * concatenated Response.
 */
static int wintertc_proxy_concurrent_on_both_fulfilled(
		jsval_region_t *region, size_t argc, const jsval_t *argv,
		jsval_t *result_ptr, jsmethod_error_t *error)
{
	jsval_t responses;
	jsval_t resp_a, resp_b;
	jsval_t body_a_promise, body_b_promise;
	jsval_promise_state_t state;
	jsval_t body_a, body_b;
	size_t body_a_len = 0, body_b_len = 0;
	uint8_t *body_a_bytes, *body_b_bytes;
	jsval_t combined_buffer;
	uint8_t *combined_bytes;
	size_t combined_cap;
	jsval_t init_value;
	jsval_t response_value;

	(void)error;

	if (argc < 1 || result_ptr == NULL) {
		errno = EINVAL;
		return -1;
	}
	responses = argv[0];
	if (responses.kind != JSVAL_KIND_ARRAY) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_array_get(region, responses, 0, &resp_a) < 0) {
		return -1;
	}
	if (jsval_array_get(region, responses, 1, &resp_b) < 0) {
		return -1;
	}
	if (resp_a.kind != JSVAL_KIND_RESPONSE ||
			resp_b.kind != JSVAL_KIND_RESPONSE) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_response_array_buffer(region, resp_a, &body_a_promise) < 0) {
		return -1;
	}
	if (jsval_promise_state(region, body_a_promise, &state) < 0 ||
			state != JSVAL_PROMISE_STATE_FULFILLED) {
		errno = EIO;
		return -1;
	}
	if (jsval_promise_result(region, body_a_promise, &body_a) < 0) {
		return -1;
	}

	if (jsval_response_array_buffer(region, resp_b, &body_b_promise) < 0) {
		return -1;
	}
	if (jsval_promise_state(region, body_b_promise, &state) < 0 ||
			state != JSVAL_PROMISE_STATE_FULFILLED) {
		errno = EIO;
		return -1;
	}
	if (jsval_promise_result(region, body_b_promise, &body_b) < 0) {
		return -1;
	}

	if (jsval_array_buffer_byte_length(region, body_a, &body_a_len) < 0) {
		return -1;
	}
	if (jsval_array_buffer_byte_length(region, body_b, &body_b_len) < 0) {
		return -1;
	}
	if (jsval_array_buffer_bytes_mut(region, body_a, &body_a_bytes,
			&body_a_len) < 0) {
		return -1;
	}
	if (jsval_array_buffer_bytes_mut(region, body_b, &body_b_bytes,
			&body_b_len) < 0) {
		return -1;
	}

	if (jsval_array_buffer_new(region,
			body_a_len + body_b_len == 0 ? 1 : body_a_len + body_b_len,
			&combined_buffer) < 0) {
		return -1;
	}
	if (jsval_array_buffer_bytes_mut(region, combined_buffer,
			&combined_bytes, &combined_cap) < 0) {
		return -1;
	}
	if (body_a_len > 0) {
		memcpy(combined_bytes, body_a_bytes, body_a_len);
	}
	if (body_b_len > 0) {
		memcpy(combined_bytes + body_a_len, body_b_bytes, body_b_len);
	}

	if (jsval_object_new(region, 1, &init_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"status", 6, jsval_number(200.0)) < 0) {
		return -1;
	}

	if (jsval_response_new(region, combined_buffer, 1, init_value, 1,
			&response_value) < 0) {
		return -1;
	}

	*result_ptr = response_value;
	return 0;
}

static int wintertc_proxy_concurrent_start_fetch(jsval_region_t *region,
		jsval_t base_value, const uint8_t *path, size_t path_len,
		jsval_t *promise_out)
{
	jsval_t path_value;
	jsval_t url_value;
	jsval_t concat_args[1];
	jsmethod_error_t error;

	if (jsval_string_new_utf8(region, path, path_len, &path_value) < 0) {
		return -1;
	}
	concat_args[0] = path_value;
	memset(&error, 0, sizeof(error));
	if (jsval_method_string_concat(region, base_value, 1, concat_args,
			&url_value, &error) < 0) {
		return -1;
	}
	return jsval_fetch(region, url_value, jsval_undefined(), 0, promise_out);
}

/*
 * Entrypoint matching faas_fetch_handler_fn. Kicks off two upstream
 * fetches in parallel, joins them via jsval_promise_all, installs a
 * continuation that concatenates bodies, returns the downstream
 * Promise to the embedder.
 */
int wintertc_proxy_concurrent_fetch_handler(jsval_region_t *region,
		jsval_t request_value, jsval_t *response_promise_out)
{
	jsval_t base_value;
	jsval_t inputs[2];
	jsval_t all_promise;
	jsval_t continuation_fn;
	jsval_t downstream_promise;

	(void)request_value;

	if (response_promise_out == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_string_new_utf8(region,
			(const uint8_t *)WINTERTC_PROXY_UPSTREAM_BASE,
			WINTERTC_PROXY_UPSTREAM_BASE_LEN, &base_value) < 0) {
		return -1;
	}
	if (wintertc_proxy_concurrent_start_fetch(region, base_value,
			wintertc_proxy_concurrent_path_a,
			sizeof(wintertc_proxy_concurrent_path_a) - 1,
			&inputs[0]) < 0) {
		return -1;
	}
	if (wintertc_proxy_concurrent_start_fetch(region, base_value,
			wintertc_proxy_concurrent_path_b,
			sizeof(wintertc_proxy_concurrent_path_b) - 1,
			&inputs[1]) < 0) {
		return -1;
	}

	if (jsval_promise_all(region, inputs, 2, &all_promise) < 0) {
		return -1;
	}
	if (jsval_function_new(region,
			wintertc_proxy_concurrent_on_both_fulfilled, 1, 0,
			jsval_undefined(), &continuation_fn) < 0) {
		return -1;
	}
	if (jsval_promise_then(region, all_promise, continuation_fn,
			jsval_undefined(), &downstream_promise) < 0) {
		return -1;
	}

	*response_promise_out = downstream_promise;
	return 0;
}

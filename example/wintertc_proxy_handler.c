#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "jsmethod.h"
#include "jsval.h"

/*
 * Source JS: example/wintertc_proxy.js
 *
 *   const JSMX_UPSTREAM_BASE = "http://example.com";
 *   export default {
 *     async fetch(request) {
 *       const upstream = await fetch(JSMX_UPSTREAM_BASE + request.url);
 *       const body = await upstream.arrayBuffer();
 *       return new Response(body, {
 *         status: upstream.status,
 *         statusText: upstream.statusText,
 *         headers: upstream.headers,
 *       });
 *     },
 *   };
 *
 * Behavior: a hosted FaaS handler that proxies the inbound Request to
 * a hardcoded upstream base URL and returns the upstream Response
 * unchanged. `request.url` is assumed to be path-and-query (matching
 * mnvkd's struct request::uri shape); it is appended to the upstream
 * base rather than parsed through `new URL(...)`.
 *
 * Lowering class: production_slow_path.
 *
 *   - The outer `await fetch(...)` is lowered via CPS: jsval_fetch()
 *     returns a pending Promise, we register
 *     `wintertc_proxy_on_upstream_fulfilled` on it via
 *     jsval_promise_then(), and the downstream Promise from then() is
 *     what the handler hands back to the embedder. When the FaaS
 *     bridge's drain pumps the FETCH_REQUEST microtask and the
 *     upstream Promise fulfills, the continuation runs and its return
 *     value fulfills the downstream (the handler's output Promise).
 *
 *   - The inner `await upstream.arrayBuffer()` is lowered synchronously
 *     because the demo transport (mock or the mnvkd vk_fetch
 *     plugin) builds Responses with an eager, already-in-region
 *     ArrayBuffer body. jsval_response_array_buffer() therefore
 *     returns a Promise that is already FULFILLED, and the continuation
 *     extracts the body inline — the same idiom
 *     vk_test_faas_demo.c's hello_world_handler uses for
 *     jsval_request_text(). If a future transport landed
 *     streaming-bodied Responses, this slot would grow a second
 *     CPS continuation.
 *
 * Host assumptions:
 *   - The embedder has registered a jsval_fetch_transport_t on the
 *     region via jsval_region_set_fetch_transport() before invoking
 *     the handler. Without one, jsval_fetch() falls back to its
 *     reject-stub and the downstream Promise rejects with
 *     TypeError("network not implemented yet").
 *   - Request bodies are read in the embedder before the handler
 *     runs (e.g. via jsval_request_prewarm_body). This handler does
 *     not touch the inbound body.
 *   - JSMX_UPSTREAM_BASE is baked in at transpile time — the
 *     transpile output *is* the deployment config.
 */

/* Upstream base URL — baked in at transpile time. Switch to e.g.
 * http://127.0.0.1:9000 to smoke-test against a local python3
 * http.server; the checked-in default points at example.com. */
#define WINTERTC_PROXY_UPSTREAM_BASE "http://example.com"
#define WINTERTC_PROXY_UPSTREAM_BASE_LEN \
	(sizeof(WINTERTC_PROXY_UPSTREAM_BASE) - 1)

/*
 * CPS continuation invoked when the upstream fetch Promise fulfills.
 * argv[0] is the upstream Response. Returns the caller's new Response
 * via *result_ptr, which becomes the fulfillment value of the outer
 * downstream Promise the handler handed back to the embedder.
 */
static int wintertc_proxy_on_upstream_fulfilled(jsval_region_t *region,
		size_t argc, const jsval_t *argv, jsval_t *result_ptr,
		jsmethod_error_t *error)
{
	jsval_t upstream;
	jsval_t body_promise;
	jsval_promise_state_t body_state;
	jsval_t body_value;
	uint32_t status = 0;
	jsval_t status_text_value;
	jsval_t headers_value;
	jsval_t init_value;
	jsval_t response_value;

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

	/* await upstream.arrayBuffer() — resolved eagerly for in-memory
	 * Response bodies. */
	if (jsval_response_array_buffer(region, upstream, &body_promise) < 0) {
		return -1;
	}
	if (jsval_promise_state(region, body_promise, &body_state) < 0) {
		return -1;
	}
	if (body_state != JSVAL_PROMISE_STATE_FULFILLED) {
		/* Streaming body path — not reachable from the current demo
		 * transports. A future slice lowers this into a second CPS
		 * continuation registered on body_promise. */
		errno = EIO;
		return -1;
	}
	if (jsval_promise_result(region, body_promise, &body_value) < 0) {
		return -1;
	}

	/* Extract upstream.status / statusText / headers. */
	if (jsval_response_status(region, upstream, &status) < 0) {
		return -1;
	}
	if (jsval_response_status_text(region, upstream, &status_text_value) < 0) {
		return -1;
	}
	if (jsval_response_headers(region, upstream, &headers_value) < 0) {
		return -1;
	}

	/* Build init dict: { status, statusText, headers }. */
	if (jsval_object_new(region, 3, &init_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"status", 6,
			jsval_number((double)status)) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"statusText", 10, status_text_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"headers", 7, headers_value) < 0) {
		return -1;
	}

	/* return new Response(body, init); */
	if (jsval_response_new(region, body_value, 1, init_value, 1,
			&response_value) < 0) {
		return -1;
	}

	*result_ptr = response_value;
	return 0;
}

/*
 * Entrypoint matching faas_fetch_handler_fn. Kicks off the upstream
 * fetch, installs the CPS continuation, and returns the downstream
 * Promise to the embedder.
 */
int wintertc_proxy_fetch_handler(jsval_region_t *region,
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

	/* const upstream = await fetch(JSMX_UPSTREAM_BASE + request.url); */
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

	/* Install the CPS continuation. The downstream Promise is the
	 * handler's output. */
	if (jsval_function_new(region, wintertc_proxy_on_upstream_fulfilled,
			1, 0, jsval_undefined(), &continuation_fn) < 0) {
		return -1;
	}
	if (jsval_promise_then(region, upstream_promise, continuation_fn,
			jsval_undefined(), &downstream_promise) < 0) {
		return -1;
	}

	*response_promise_out = downstream_promise;
	return 0;
}

#ifndef JSMX_RUNTIME_MODULES_SHARED_FAAS_BRIDGE_H
#define JSMX_RUNTIME_MODULES_SHARED_FAAS_BRIDGE_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "jsval.h"

/*
 * FaaS handler bridge — reusable glue between a jsmx hosted-function
 * handler and an HTTP/1.1 embedder.
 *
 * Every hosted function — whether hand-written C or a transpiled
 * `export default { async fetch(request) { ... } }` — must match
 * `faas_fetch_handler_fn`. The embedder builds a Request value,
 * calls the handler, drives `jsval_microtask_drain` until the
 * returned Promise settles, then serializes the resulting Response
 * into wire bytes with `faas_serialize_response`.
 *
 * This file is header-only. It follows the same pattern as
 * `runtime_modules/shared/fs_sync.h` and
 * `runtime_modules/shared/webcrypto_openssl.h`.
 */

typedef int (*faas_fetch_handler_fn)(jsval_region_t *region,
		jsval_t request_value, jsval_t *response_promise_out);

/*
 * Pump the microtask queue until `promise_value` settles (fulfilled
 * or rejected). Returns 0 once the promise is settled, -1 with errno
 * set on fatal error:
 *
 *   - EDEADLK if the promise is pending and the queue is empty
 *     (handler forgot to resolve).
 *   - Whatever errno `jsval_microtask_drain` sets on its own failure.
 *
 * Body sources that yield (e.g. an mnvkd vk_socket reader) do so
 * inside their own read callback; the drain loop is oblivious to
 * that and just keeps calling jsval_microtask_drain.
 */
static int faas_drain_until_settled(jsval_region_t *region,
		jsval_t promise_value, jsmethod_error_t *error)
{
	jsval_promise_state_t state;

	if (region == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (promise_value.kind != JSVAL_KIND_PROMISE) {
		errno = EINVAL;
		return -1;
	}
	while (1) {
		if (jsval_promise_state(region, promise_value, &state) < 0) {
			return -1;
		}
		if (state != JSVAL_PROMISE_STATE_PENDING) {
			return 0;
		}
		if (jsval_microtask_pending(region) == 0) {
			errno = EDEADLK;
			return -1;
		}
		if (jsval_microtask_drain(region, error) < 0) {
			return -1;
		}
	}
}

/*
 * ASCII lowercase comparison helper for "skip this header during
 * serialization" checks. Case-insensitive over token bytes only.
 */
static int faas_ascii_ci_equal(const uint8_t *a, size_t a_len,
		const char *b)
{
	size_t i;
	size_t b_len = strlen(b);

	if (a_len != b_len) {
		return 0;
	}
	for (i = 0; i < a_len; i++) {
		uint8_t ca = a[i];
		uint8_t cb = (uint8_t)b[i];
		if (ca >= 'A' && ca <= 'Z') { ca = (uint8_t)(ca + 32); }
		if (cb >= 'A' && cb <= 'Z') { cb = (uint8_t)(cb + 32); }
		if (ca != cb) { return 0; }
	}
	return 1;
}

/*
 * Append `bytes[0..len)` into `out_buf + *pos`, growing `*pos` by
 * `len`. Returns 0 on success, -1 with errno=ENOBUFS if the buffer
 * is too small (in which case `*pos` still advances by `len` so the
 * caller can see the required size when out_len is read later).
 */
static int faas_emit_bytes(uint8_t *out_buf, size_t out_cap, size_t *pos,
		const uint8_t *bytes, size_t len)
{
	if (out_buf != NULL && *pos + len <= out_cap) {
		memcpy(out_buf + *pos, bytes, len);
	}
	*pos += len;
	return 0;
}

static int faas_emit_literal(uint8_t *out_buf, size_t out_cap, size_t *pos,
		const char *literal)
{
	return faas_emit_bytes(out_buf, out_cap, pos, (const uint8_t *)literal,
			strlen(literal));
}

/*
 * Serialize a jsval Response into an HTTP/1.1 response byte stream:
 *
 *   HTTP/1.1 <status> <statusText>\r\n
 *   Header-Name: value\r\n
 *   ...
 *   Content-Length: <n>\r\n
 *   \r\n
 *   <body bytes>
 *
 * Writes up to `out_cap` bytes into `out_buf`. On return, `*out_len`
 * holds the total size that *would* have been written, so a caller
 * that passes too small a buffer can read the required size from
 * `*out_len` and retry with a larger allocation.
 *
 * Returns 0 on success, -1 with errno on error:
 *   - ENOBUFS: buffer was too small; *out_len holds required size.
 *   - ENOTSUP: Response has a streaming body source (deferred).
 *   - EINVAL : Response.error() (status=0, not serializable), or
 *              kind mismatch, or NULL out_len.
 *
 * Rules:
 *   - Always emits Content-Length computed from the body buffer,
 *     overriding any caller-set Content-Length header.
 *   - Skips any caller-set Content-Length or Transfer-Encoding
 *     header during iteration.
 *   - statusText defaults to the empty string if unset; we emit
 *     "HTTP/1.1 <status> <statusText>\r\n" with a single space
 *     between status and reason phrase.
 */
static int faas_serialize_response(jsval_region_t *region,
		jsval_t response_value, uint8_t *out_buf, size_t out_cap,
		size_t *out_len)
{
	uint32_t status = 0;
	jsval_t status_text_value;
	jsval_t headers_value;
	jsval_t body_buffer = jsval_undefined();
	size_t body_len = 0;
	size_t header_count = 0;
	size_t status_text_len = 0;
	size_t pos = 0;
	char numbuf[32];
	int numlen;
	size_t i;

	if (region == NULL || out_len == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (response_value.kind != JSVAL_KIND_RESPONSE) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_response_status(region, response_value, &status) < 0) {
		return -1;
	}
	if (status == 0) {
		/* Response.error() — not a wire-serializable response. */
		errno = EINVAL;
		return -1;
	}
	if (jsval_response_status_text(region, response_value,
			&status_text_value) < 0) {
		return -1;
	}
	if (jsval_response_headers(region, response_value, &headers_value) < 0) {
		return -1;
	}
	if (jsval_response_body_snapshot(region, response_value,
			&body_buffer) < 0) {
		return -1;
	}
	if (body_buffer.kind == JSVAL_KIND_ARRAY_BUFFER) {
		if (jsval_array_buffer_byte_length(region, body_buffer,
				&body_len) < 0) {
			return -1;
		}
	}

	/* Status line: "HTTP/1.1 <status> <statusText>\r\n" */
	if (faas_emit_literal(out_buf, out_cap, &pos, "HTTP/1.1 ") < 0) {
		return -1;
	}
	numlen = snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)status);
	if (numlen < 0) {
		errno = EINVAL;
		return -1;
	}
	faas_emit_bytes(out_buf, out_cap, &pos, (const uint8_t *)numbuf,
			(size_t)numlen);
	faas_emit_literal(out_buf, out_cap, &pos, " ");
	if (status_text_value.kind == JSVAL_KIND_STRING_JSSTR8) {
		const uint8_t *stbytes = NULL;
		if (jsval_string_jsstr8_bytes(region, status_text_value,
				&stbytes, &status_text_len) < 0) {
			return -1;
		}
		if (status_text_len > 0) {
			faas_emit_bytes(out_buf, out_cap, &pos, stbytes, status_text_len);
		}
	} else if (status_text_value.kind == JSVAL_KIND_STRING) {
		if (jsval_string_copy_utf8(region, status_text_value, NULL, 0,
				&status_text_len) < 0) {
			return -1;
		}
		if (status_text_len > 0) {
			uint8_t stbuf[status_text_len];
			if (jsval_string_copy_utf8(region, status_text_value, stbuf,
					status_text_len, NULL) < 0) {
				return -1;
			}
			faas_emit_bytes(out_buf, out_cap, &pos, stbuf, status_text_len);
		}
	}
	faas_emit_literal(out_buf, out_cap, &pos, "\r\n");

	/* Headers in insertion order, skipping Content-Length and
	 * Transfer-Encoding — we own those. */
	if (jsval_headers_size(region, headers_value, &header_count) < 0) {
		return -1;
	}
	for (i = 0; i < header_count; i++) {
		jsval_t name_val;
		jsval_t value_val;
		size_t name_len = 0;
		size_t value_len = 0;

		if (jsval_headers_entry_at(region, headers_value, i, &name_val,
				&value_val) < 0) {
			return -1;
		}
		{
			const uint8_t *name_bytes = NULL;
			const uint8_t *value_bytes = NULL;
			int name_borrowed = 0;
			int value_borrowed = 0;

			/* Borrow JSSTR8 inline bytes (zero-copy) when available;
			 * otherwise measure-then-copy the UTF-16 STRING. */
			if (name_val.kind == JSVAL_KIND_STRING_JSSTR8) {
				if (jsval_string_jsstr8_bytes(region, name_val,
						&name_bytes, &name_len) < 0) {
					return -1;
				}
				name_borrowed = 1;
			} else {
				if (jsval_string_copy_utf8(region, name_val, NULL, 0,
						&name_len) < 0) {
					return -1;
				}
			}
			if (value_val.kind == JSVAL_KIND_STRING_JSSTR8) {
				if (jsval_string_jsstr8_bytes(region, value_val,
						&value_bytes, &value_len) < 0) {
					return -1;
				}
				value_borrowed = 1;
			} else {
				if (jsval_string_copy_utf8(region, value_val, NULL, 0,
						&value_len) < 0) {
					return -1;
				}
			}
			{
				uint8_t name_scratch[name_borrowed ? 1
						: (name_len ? name_len : 1)];
				uint8_t value_scratch[value_borrowed ? 1
						: (value_len ? value_len : 1)];

				if (!name_borrowed && name_len > 0) {
					if (jsval_string_copy_utf8(region, name_val,
							name_scratch, name_len, NULL) < 0) {
						return -1;
					}
					name_bytes = name_scratch;
				}
				if (!value_borrowed && value_len > 0) {
					if (jsval_string_copy_utf8(region, value_val,
							value_scratch, value_len, NULL) < 0) {
						return -1;
					}
					value_bytes = value_scratch;
				}
				if (faas_ascii_ci_equal(name_bytes, name_len,
								"content-length")
						|| faas_ascii_ci_equal(name_bytes, name_len,
								"transfer-encoding")) {
					continue;
				}
				faas_emit_bytes(out_buf, out_cap, &pos, name_bytes,
						name_len);
				faas_emit_literal(out_buf, out_cap, &pos, ": ");
				if (value_len > 0) {
					faas_emit_bytes(out_buf, out_cap, &pos, value_bytes,
							value_len);
				}
				faas_emit_literal(out_buf, out_cap, &pos, "\r\n");
			}
		}
	}

	/* Authoritative Content-Length. */
	numlen = snprintf(numbuf, sizeof(numbuf), "Content-Length: %zu\r\n",
			body_len);
	if (numlen < 0) {
		errno = EINVAL;
		return -1;
	}
	faas_emit_bytes(out_buf, out_cap, &pos, (const uint8_t *)numbuf,
			(size_t)numlen);
	faas_emit_literal(out_buf, out_cap, &pos, "\r\n");

	/* Body bytes. */
	if (body_len > 0) {
		if (out_buf != NULL && pos + body_len <= out_cap) {
			if (jsval_array_buffer_copy_bytes(region, body_buffer,
					out_buf + pos, body_len, NULL) < 0) {
				return -1;
			}
		}
		pos += body_len;
	}

	*out_len = pos;
	if (pos > out_cap) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

/*
 * Phase 3c-8: serialize the response status line + headers + the
 * end-of-headers CRLF for a chunked-body response. Strips
 * Content-Length and Transfer-Encoding from the handler-supplied
 * headers (we own framing) and emits exactly one
 * "Transfer-Encoding: chunked\r\n" line. Body is NOT emitted —
 * the caller streams it as chunked frames.
 *
 * Same 2-pass measure-then-fill discipline as
 * faas_serialize_response: pass NULL/0 to measure, allocate, pass
 * the buffer to fill.
 */
static int faas_serialize_response_headers_chunked(jsval_region_t *region,
		jsval_t response_value, uint8_t *out_buf, size_t out_cap,
		size_t *out_len)
{
	uint32_t status = 0;
	jsval_t status_text_value;
	jsval_t headers_value;
	size_t header_count = 0;
	size_t status_text_len = 0;
	size_t pos = 0;
	char numbuf[32];
	int numlen;
	size_t i;

	if (region == NULL || out_len == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (response_value.kind != JSVAL_KIND_RESPONSE) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_response_status(region, response_value, &status) < 0) {
		return -1;
	}
	if (status == 0) {
		errno = EINVAL;
		return -1;
	}
	if (jsval_response_status_text(region, response_value,
			&status_text_value) < 0) {
		return -1;
	}
	if (jsval_response_headers(region, response_value, &headers_value) < 0) {
		return -1;
	}

	if (faas_emit_literal(out_buf, out_cap, &pos, "HTTP/1.1 ") < 0) {
		return -1;
	}
	numlen = snprintf(numbuf, sizeof(numbuf), "%u", (unsigned)status);
	if (numlen < 0) { errno = EINVAL; return -1; }
	faas_emit_bytes(out_buf, out_cap, &pos, (const uint8_t *)numbuf,
			(size_t)numlen);
	faas_emit_literal(out_buf, out_cap, &pos, " ");
	if (status_text_value.kind == JSVAL_KIND_STRING_JSSTR8) {
		const uint8_t *stbytes = NULL;
		if (jsval_string_jsstr8_bytes(region, status_text_value,
				&stbytes, &status_text_len) < 0) {
			return -1;
		}
		if (status_text_len > 0) {
			faas_emit_bytes(out_buf, out_cap, &pos, stbytes, status_text_len);
		}
	} else if (status_text_value.kind == JSVAL_KIND_STRING) {
		if (jsval_string_copy_utf8(region, status_text_value, NULL, 0,
				&status_text_len) < 0) {
			return -1;
		}
		if (status_text_len > 0) {
			uint8_t stbuf[status_text_len];
			if (jsval_string_copy_utf8(region, status_text_value, stbuf,
					status_text_len, NULL) < 0) {
				return -1;
			}
			faas_emit_bytes(out_buf, out_cap, &pos, stbuf, status_text_len);
		}
	}
	faas_emit_literal(out_buf, out_cap, &pos, "\r\n");

	if (jsval_headers_size(region, headers_value, &header_count) < 0) {
		return -1;
	}
	for (i = 0; i < header_count; i++) {
		jsval_t name_val;
		jsval_t value_val;
		size_t name_len = 0;
		size_t value_len = 0;

		if (jsval_headers_entry_at(region, headers_value, i, &name_val,
				&value_val) < 0) {
			return -1;
		}
		{
			const uint8_t *name_bytes = NULL;
			const uint8_t *value_bytes = NULL;
			int name_borrowed = 0;
			int value_borrowed = 0;

			/* Borrow JSSTR8 inline bytes (zero-copy) when available;
			 * otherwise measure-then-copy the UTF-16 STRING. */
			if (name_val.kind == JSVAL_KIND_STRING_JSSTR8) {
				if (jsval_string_jsstr8_bytes(region, name_val,
						&name_bytes, &name_len) < 0) {
					return -1;
				}
				name_borrowed = 1;
			} else {
				if (jsval_string_copy_utf8(region, name_val, NULL, 0,
						&name_len) < 0) {
					return -1;
				}
			}
			if (value_val.kind == JSVAL_KIND_STRING_JSSTR8) {
				if (jsval_string_jsstr8_bytes(region, value_val,
						&value_bytes, &value_len) < 0) {
					return -1;
				}
				value_borrowed = 1;
			} else {
				if (jsval_string_copy_utf8(region, value_val, NULL, 0,
						&value_len) < 0) {
					return -1;
				}
			}
			{
				uint8_t name_scratch[name_borrowed ? 1
						: (name_len ? name_len : 1)];
				uint8_t value_scratch[value_borrowed ? 1
						: (value_len ? value_len : 1)];

				if (!name_borrowed && name_len > 0) {
					if (jsval_string_copy_utf8(region, name_val,
							name_scratch, name_len, NULL) < 0) {
						return -1;
					}
					name_bytes = name_scratch;
				}
				if (!value_borrowed && value_len > 0) {
					if (jsval_string_copy_utf8(region, value_val,
							value_scratch, value_len, NULL) < 0) {
						return -1;
					}
					value_bytes = value_scratch;
				}
				if (faas_ascii_ci_equal(name_bytes, name_len,
								"content-length")
						|| faas_ascii_ci_equal(name_bytes, name_len,
								"transfer-encoding")) {
					continue;
				}
				faas_emit_bytes(out_buf, out_cap, &pos, name_bytes,
						name_len);
				faas_emit_literal(out_buf, out_cap, &pos, ": ");
				if (value_len > 0) {
					faas_emit_bytes(out_buf, out_cap, &pos, value_bytes,
							value_len);
				}
				faas_emit_literal(out_buf, out_cap, &pos, "\r\n");
			}
		}
	}

	faas_emit_literal(out_buf, out_cap, &pos,
			"Transfer-Encoding: chunked\r\n");
	faas_emit_literal(out_buf, out_cap, &pos, "\r\n");

	*out_len = pos;
	if (pos > out_cap) {
		errno = ENOBUFS;
		return -1;
	}
	return 0;
}

/* =========================================================================
 * Higher-level glue: assemble a Request, run a handler, extract a Response.
 *
 * These helpers keep an embedder's per-request plumbing to a minimum. The
 * mnvkd FaaS demo uses all three; a future standalone embedder (libevent,
 * or a transpiler-generated main) can use them unchanged because none of
 * them touch mnvkd types.
 * ========================================================================= */

/*
 * One header pair to hand into faas_build_request_from_parts(). The caller
 * owns the underlying bytes; the helper copies them into the region via
 * jsval_string_new_utf8 before returning, so the caller is free to reuse
 * or release its source storage afterward.
 */
typedef struct faas_header_pair_s {
	const uint8_t *name;
	size_t name_len;
	const uint8_t *value;
	size_t value_len;
} faas_header_pair_t;

/*
 * Assemble a jsval Request from raw parts via the init-dict constructor.
 *
 * method        — NUL-terminated ASCII method name ("GET", "POST", ...).
 *                 Pass NULL or "" to default to GET.
 * url_bytes     — URL path-and-query (NOT NUL-terminated).
 * url_len       — length of url_bytes in bytes.
 * headers       — array of header pairs; header_count may be zero.
 * body_bytes    — raw body bytes; may be NULL iff body_len == 0.
 * body_len      — length of body_bytes in bytes.
 *
 * Returns 0 and fills *request_out on success. Returns -1 with errno set
 * on failure (propagated from the underlying jsmx call).
 *
 * Internally builds a Headers object, populates it via jsval_headers_append,
 * wraps the body as a jsval string (byte-preserving, no UTF-8 validation),
 * constructs the init dict, and calls jsval_request_new. All intermediate
 * allocations live in the caller-supplied region.
 */
static int faas_build_request_from_parts(jsval_region_t *region,
		const char *method,
		const uint8_t *url_bytes, size_t url_len,
		const faas_header_pair_t *headers, size_t header_count,
		const uint8_t *body_bytes, size_t body_len,
		jsval_t *request_out)
{
	jsval_t headers_value;
	jsval_t url_value;
	jsval_t init_value;
	jsval_t method_value;
	jsval_t name_val;
	jsval_t value_val;
	jsval_t body_value;
	const char *method_name = (method != NULL && method[0] != '\0')
			? method : "GET";
	size_t i;

	if (region == NULL || request_out == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (header_count > 0 && headers == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (body_len > 0 && body_bytes == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (jsval_headers_new(region, JSVAL_HEADERS_GUARD_REQUEST,
			&headers_value) < 0) {
		return -1;
	}
	for (i = 0; i < header_count; i++) {
		if (jsval_string_new_utf8(region, headers[i].name,
				headers[i].name_len, &name_val) < 0) {
			return -1;
		}
		if (jsval_string_new_utf8(region, headers[i].value,
				headers[i].value_len, &value_val) < 0) {
			return -1;
		}
		/* Silently skip headers that fail validation (e.g. bytes that
		 * aren't valid HTTP tokens). Real clients occasionally send
		 * oddities; aborting the whole request is too strict. */
		(void)jsval_headers_append(region, headers_value, name_val,
				value_val);
	}

	if (jsval_string_new_utf8(region, url_bytes, url_len, &url_value) < 0) {
		return -1;
	}
	if (jsval_string_new_utf8(region, (const uint8_t *)method_name,
			strlen(method_name), &method_value) < 0) {
		return -1;
	}

	if (jsval_object_new(region, 4, &init_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"method", 6, method_value) < 0) {
		return -1;
	}
	if (jsval_object_set_utf8(region, init_value,
			(const uint8_t *)"headers", 7, headers_value) < 0) {
		return -1;
	}
	if (body_len > 0) {
		if (jsval_string_new_utf8(region, body_bytes, body_len,
				&body_value) < 0) {
			return -1;
		}
		if (jsval_object_set_utf8(region, init_value,
				(const uint8_t *)"body", 4, body_value) < 0) {
			return -1;
		}
	}

	return jsval_request_new(region, url_value, init_value, 1, request_out);
}

/*
 * Call a handler, drive microtask drain until its returned promise settles,
 * and extract the resolved value.
 *
 * On success (promise fulfilled with a Response-shaped value):
 *   returns 0, *response_out is a JSVAL_KIND_RESPONSE jsval.
 *
 * On handler rejection (promise rejected with any reason):
 *   returns -1, errno is set to EIO, and *response_out holds the rejection
 *   reason. Typical reasons are DOMException jsvals with name/message
 *   fields the caller can inspect to build a custom 500 Response.
 *
 * On handler invocation failure (handler returned -1 directly):
 *   returns -1, errno preserved from the handler; *response_out is
 *   undefined.
 *
 * On drain failure (e.g. deadlock):
 *   returns -1, errno set by faas_drain_until_settled.
 */
static int faas_run_and_extract(jsval_region_t *region,
		faas_fetch_handler_fn handler,
		jsval_t request_value,
		jsval_t *response_out,
		jsmethod_error_t *error)
{
	jsval_t response_promise;
	jsval_promise_state_t state;

	if (region == NULL || handler == NULL || response_out == NULL) {
		errno = EINVAL;
		return -1;
	}

	if (handler(region, request_value, &response_promise) < 0) {
		return -1;
	}
	if (faas_drain_until_settled(region, response_promise, error) < 0) {
		return -1;
	}
	if (jsval_promise_state(region, response_promise, &state) < 0) {
		return -1;
	}
	if (state == JSVAL_PROMISE_STATE_REJECTED) {
		jsval_t reason;
		if (jsval_promise_result(region, response_promise, &reason) < 0) {
			return -1;
		}
		*response_out = reason;
		errno = EIO;
		return -1;
	}
	if (state != JSVAL_PROMISE_STATE_FULFILLED) {
		errno = EIO;
		return -1;
	}
	return jsval_promise_result(region, response_promise, response_out);
}

#endif

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
	if (status_text_value.kind == JSVAL_KIND_STRING) {
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
		if (jsval_string_copy_utf8(region, name_val, NULL, 0,
				&name_len) < 0) {
			return -1;
		}
		if (jsval_string_copy_utf8(region, value_val, NULL, 0,
				&value_len) < 0) {
			return -1;
		}
		{
			uint8_t name_buf[name_len ? name_len : 1];
			uint8_t value_buf[value_len ? value_len : 1];

			if (name_len > 0
					&& jsval_string_copy_utf8(region, name_val, name_buf,
							name_len, NULL) < 0) {
				return -1;
			}
			if (value_len > 0
					&& jsval_string_copy_utf8(region, value_val, value_buf,
							value_len, NULL) < 0) {
				return -1;
			}
			if (faas_ascii_ci_equal(name_buf, name_len, "content-length")
					|| faas_ascii_ci_equal(name_buf, name_len,
							"transfer-encoding")) {
				continue;
			}
			faas_emit_bytes(out_buf, out_cap, &pos, name_buf, name_len);
			faas_emit_literal(out_buf, out_cap, &pos, ": ");
			if (value_len > 0) {
				faas_emit_bytes(out_buf, out_cap, &pos, value_buf,
						value_len);
			}
			faas_emit_literal(out_buf, out_cap, &pos, "\r\n");
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

#endif

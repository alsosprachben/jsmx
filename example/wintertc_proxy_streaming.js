/*
 * WinterTC streaming-proxy demo — same shape as wintertc_proxy.js but
 * consumes the upstream body via `response.body.getReader().read()`
 * in a CPS-style chain instead of awaiting `arrayBuffer()`.
 *
 * The companion file example/wintertc_proxy_streaming_handler.c is the
 * hand-lowered C output of the jsmx-transpile-program skill for this
 * source. The pair exists so the skill's "Streaming-Body Read Loop"
 * recipe in lowering-recipes.md has a worked reference: each
 * `await reader.read()` lowers to a separate native-function
 * continuation registered on the read Promise via
 * jsval_promise_then.
 *
 * Behavior identical to wintertc_proxy.js — proxies inbound Request
 * to a hardcoded upstream base, returns the upstream Response with
 * the body bytes accumulated through the stream API.
 */

const JSMX_UPSTREAM_BASE = "http://example.com";

export default {
  async fetch(request) {
    const upstream = await fetch(JSMX_UPSTREAM_BASE + request.url);
    const reader = upstream.body.getReader();

    const chunks = [];
    let total = 0;
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      chunks.push(value);
      total += value.length;
    }

    const out = new Uint8Array(total);
    let offset = 0;
    for (const chunk of chunks) {
      out.set(chunk, offset);
      offset += chunk.length;
    }

    return new Response(out, {
      status: upstream.status,
      statusText: upstream.statusText,
      headers: upstream.headers,
    });
  },
};

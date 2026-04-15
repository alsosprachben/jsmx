/*
 * WinterTC proxy demo — the canonical cloud-function shape.
 *
 * Receives an inbound Request, forwards it to a hardcoded upstream base
 * URL, and returns the upstream Response unchanged (status, statusText,
 * headers, body). Exercises the fetch server interface (FaaS handler
 * contract) and the fetch client interface (outbound fetch via the
 * registered jsval_fetch_transport_t) in a single round trip.
 *
 * The companion file example/wintertc_proxy_handler.c is the
 * hand-lowered C output of the jsmx-transpile-program skill for this
 * source. It is tracked in the repo so the transpile lowering rules
 * have a reference output to diff against.
 *
 * Build / link: wintertc_proxy_handler.c exports a
 * faas_fetch_handler_fn named wintertc_proxy_fetch_handler. The
 * embedder (mnvkd's vk_test_faas_demo, or jsmx's test_faas) links the
 * object and registers a fetch transport before invoking the handler
 * via runtime_modules/shared/faas_bridge.h.
 *
 * Assumption about request.url: the embedder passes a path-and-query
 * string (e.g. "/echo?x=1"), matching mnvkd's struct request::uri
 * shape. The handler concatenates it onto the upstream base rather
 * than routing through `new URL(...)`, which would require an absolute
 * URL.
 */

const JSMX_UPSTREAM_BASE = "http://example.com";

export default {
  async fetch(request) {
    const upstream = await fetch(JSMX_UPSTREAM_BASE + request.url);
    const body = await upstream.arrayBuffer();
    return new Response(body, {
      status: upstream.status,
      statusText: upstream.statusText,
      headers: upstream.headers,
    });
  },
};

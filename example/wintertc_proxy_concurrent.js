/*
 * WinterTC concurrent-fetch demo — Promise.all over two upstream
 * fetches, joined into a single response body.
 *
 * Receives an inbound Request, issues two parallel fetches to the
 * same upstream base (distinct paths), and returns a Response whose
 * body is the byte concatenation of both upstream bodies.
 *
 * The companion file example/wintertc_proxy_concurrent_handler.c is
 * the hand-lowered C output of the jsmx-transpile-program skill for
 * this source. It is tracked in the repo so the transpile lowering
 * rules have a reference output to diff against.
 *
 * Build / link: wintertc_proxy_concurrent_handler.c exports a
 * faas_fetch_handler_fn named wintertc_proxy_concurrent_fetch_handler.
 * The embedder (mnvkd's vk_test_faas_demo_concurrent) links the
 * object and registers a fetch transport before invoking the handler
 * via runtime_modules/shared/faas_bridge.h.
 *
 * Test fixture: the upstream (mnvkd's python3 -m http.server harness)
 * serves two files matching the hardcoded paths below. The
 * vk_test_faas_demo_concurrent.sh driver pre-seeds part-a.txt with
 * "first\n" and part-b.txt with "second\n", so the downstream body
 * is "first\nsecond\n" (13 bytes).
 *
 * Assumption about request.url: ignored. The paths are baked in at
 * transpile time — this handler's routing is static fan-out rather
 * than a URL-driven proxy.
 */

const JSMX_UPSTREAM_BASE = "http://example.com";

export default {
  async fetch(request) {
    const [a, b] = await Promise.all([
      fetch(JSMX_UPSTREAM_BASE + "/part-a.txt"),
      fetch(JSMX_UPSTREAM_BASE + "/part-b.txt"),
    ]);
    const [bodyA, bodyB] = await Promise.all([
      a.arrayBuffer(),
      b.arrayBuffer(),
    ]);
    const out = new Uint8Array(bodyA.byteLength + bodyB.byteLength);
    out.set(new Uint8Array(bodyA), 0);
    out.set(new Uint8Array(bodyB), bodyA.byteLength);
    return new Response(out.buffer, { status: 200 });
  },
};

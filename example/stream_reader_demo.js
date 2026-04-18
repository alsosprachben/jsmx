/*
 * ReadableStream reader-loop demo — the reference fixture for
 * the "Reading Response Bodies As Streams" lowering recipe in
 * skills/jsmx-transpile-program/references/lowering-recipes.md.
 *
 * The handler ignores the incoming request and builds a
 * predictable in-memory Response. It then reads that Response
 * back through the Phase-1 ReadableStream API — response.body
 * .getReader() + reader.read() loop with { value, done }
 * destructuring — concatenates the chunks into a single byte
 * buffer, and returns that buffer as the handler's Response.
 *
 * The round-trip path stays in-memory (no fetch transport,
 * no upstream server), so every reader.read() fulfills
 * synchronously and the lowering lands at
 * production_flattened. Switching the source to a streaming
 * body (future mnvkd vk_socket adapter) flips the class to
 * production_slow_path — one CPS continuation per read.
 *
 * The companion file example/stream_reader_demo_handler.c
 * is the hand-lowered C output of the jsmx-transpile-program
 * skill for this source. The pair exists so the skill's
 * ReadableStream recipe has a reference to diff against when
 * extending the lowering.
 */

const STREAM_DEMO_PAYLOAD = "hello from a readable stream";

export default {
  async fetch(request) {
    const source = new Response(STREAM_DEMO_PAYLOAD);
    const reader = source.body.getReader();

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
      status: 200,
      headers: { "content-type": "text/plain" },
    });
  },
};

// Repo-authored WritableStream basic write+close parity
// fixture (Phase 3a-2). A WritableStream wired to a
// capturing underlying sink accepts writer.write() chunks
// in order, and writer.close() drains any queued writes
// before invoking the sink's close callback. All returned
// promises fulfill with undefined; the sink observes the
// concatenated payload.

// Equivalent WHATWG shape:
//
// const sink = { chunks: [], closed: false };
// const stream = new WritableStream({
//   write(chunk) { sink.chunks.push(chunk); },
//   close() { sink.closed = true; },
// });
// const writer = stream.getWriter();
// const p1 = writer.write(new TextEncoder().encode("foo"));
// const p2 = writer.write(new TextEncoder().encode("bar"));
// const p3 = writer.write(new TextEncoder().encode("baz"));
// const pclose = writer.close();
// await Promise.all([p1, p2, p3, pclose]);
// // sink.chunks joined === "foobarbaz"
// // sink.closed === true

// Repo-authored pipeTo basic parity fixture (Phase 6-2).
// A ReadableStream over a fixed byte payload piped into a
// capturing WritableStream: the entire payload arrives at
// the sink in order and the sink's close() runs exactly
// once. The pipe Promise fulfils with undefined.

// Equivalent WHATWG shape:
//
// const sink = { chunks: [], closed: false };
// const writable = new WritableStream({
//   write(chunk) { sink.chunks.push(chunk); },
//   close() { sink.closed = true; },
// });
// const readable = new ReadableStream({
//   start(controller) {
//     controller.enqueue(
//         new TextEncoder().encode("hello, world"));
//     controller.close();
//   },
// });
// await readable.pipeTo(writable);
// // sink.chunks joined as bytes === "hello, world"
// // sink.closed === true

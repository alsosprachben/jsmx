// Repo-authored CompressionStream empty-input parity
// fixture (Phase 5-2). An empty input stream piped through
// compress + decompress yields an empty output stream --
// but the intermediate gzip envelope is still non-empty
// (~20 bytes of gzip headers + trailer), confirming
// flush() runs cleanly through both halves with no input.

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({
//   start(c) { c.close(); },  // no enqueue
// });
// const compressed = readable
//     .pipeThrough(new CompressionStream("gzip"));
// // collected compressed bytes: non-zero length (gzip envelope)
// const decompressed = compressed
//     .pipeThrough(new DecompressionStream("gzip"));
// // collected decompressed bytes: length 0

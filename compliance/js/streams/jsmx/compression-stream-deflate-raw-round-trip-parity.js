// Repo-authored CompressionStream deflate-raw round-trip
// parity fixture (Phase 5-2). Raw deflate has no wrapper;
// the bytes between compress and decompress are a bare
// deflate bitstream. This fixture verifies the round-trip
// identity only (no envelope byte to check).

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({
//   start(c) {
//     c.enqueue(new TextEncoder().encode("Hello, streams!"));
//     c.close();
//   },
// });
// const compressed = readable
//     .pipeThrough(new CompressionStream("deflate-raw"));
// const decompressed = compressed
//     .pipeThrough(new DecompressionStream("deflate-raw"));
// // decompressed bytes === "Hello, streams!"

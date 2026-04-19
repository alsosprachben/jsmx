// Repo-authored CompressionStream deflate (zlib-wrapped)
// round-trip parity fixture (Phase 5-2). Same shape as
// the gzip round-trip fixture with the "deflate" format
// selector and the zlib CMF byte 0x78 as the envelope
// marker instead of the gzip magic.

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({
//   start(c) {
//     c.enqueue(new TextEncoder().encode("Hello, streams!"));
//     c.close();
//   },
// });
// const compressed = readable
//     .pipeThrough(new CompressionStream("deflate"));
// // compressed[0] === 0x78  (zlib CMF: deflate method,
// //                          32 KB window)
// const decompressed = compressed
//     .pipeThrough(new DecompressionStream("deflate"));
// // decompressed bytes === "Hello, streams!"

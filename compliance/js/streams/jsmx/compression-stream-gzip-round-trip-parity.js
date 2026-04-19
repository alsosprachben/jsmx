// Repo-authored CompressionStream gzip round-trip parity
// fixture (Phase 5-2). The canonical WHATWG Compression
// Streams flow: a ReadableStream of bytes is piped through
// a gzip CompressionStream to produce gzip-framed bytes,
// which in turn decompress back to the original input.

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({
//   start(c) {
//     c.enqueue(new TextEncoder().encode("Hello, streams!"));
//     c.close();
//   },
// });
// const compressed = readable
//     .pipeThrough(new CompressionStream("gzip"));
// // collected bytes of `compressed` start with 0x1F 0x8B
// const decompressed = compressed
//     .pipeThrough(new DecompressionStream("gzip"));
// // collected bytes of `decompressed` === "Hello, streams!"

// Repo-authored DecompressionStream invalid-input parity
// fixture (Phase 5-2). Bytes that do not parse as the
// declared format (here: gzip) cause the readable side
// to reject with a TypeError, matching WHATWG.

// Equivalent WHATWG shape:
//
// const garbage = new Uint8Array(
//   [0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE]);
// const readable = new ReadableStream({
//   start(c) { c.enqueue(garbage); c.close(); },
// });
// const out = readable
//     .pipeThrough(new DecompressionStream("gzip"));
// try {
//   await out.getReader().read();
// } catch (e) {
//   // e is TypeError
// }

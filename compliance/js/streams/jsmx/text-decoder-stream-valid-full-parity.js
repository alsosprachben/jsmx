// Repo-authored TextDecoderStream valid-full parity
// fixture (Phase 5b-2). When the input chunk contains
// only complete UTF-8 sequences (every codepoint boundary
// aligns within the chunk), the decoder passes bytes
// through unchanged.

// Equivalent WHATWG shape:
//
// const ds = new TextDecoderStream();
// const writer = ds.writable.getWriter();
// const reader = ds.readable.getReader();
// const input = new Uint8Array([
//   0x68, 0x69, 0xCE, 0xB1, 0xCE, 0xB2,
// ]);  // "hiαβ"
// writer.write(input);
// writer.close();
// // reader yields the same bytes, then { done: true }.

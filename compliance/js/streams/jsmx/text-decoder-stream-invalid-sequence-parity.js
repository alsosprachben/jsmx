// Repo-authored TextDecoderStream invalid-sequence parity
// fixture (Phase 5b-2). A lone UTF-8 continuation byte
// (0x80) is not a valid leading byte and cannot start any
// codepoint; the decoder replaces it with the U+FFFD
// REPLACEMENT CHARACTER (encoded as the three bytes
// EF BF BD in UTF-8).

// Equivalent WHATWG shape:
//
// const ds = new TextDecoderStream();
// const writer = ds.writable.getWriter();
// const reader = ds.readable.getReader();
// writer.write(new Uint8Array([0x80]));
// writer.close();
// // reader yields Uint8Array([0xEF, 0xBF, 0xBD]),
// //   then { done: true }.

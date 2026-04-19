// Repo-authored TextDecoderStream flush-trailing parity
// fixture (Phase 5b-2). When the writable side is closed
// while the decoder still holds a partial leading sequence
// (here: a single 0xCE byte that begins a 2-byte
// codepoint), flush() emits a U+FFFD before EOF — the
// trailing bytes can never complete and the decoder must
// not silently drop them.

// Equivalent WHATWG shape:
//
// const ds = new TextDecoderStream();
// const writer = ds.writable.getWriter();
// const reader = ds.readable.getReader();
// writer.write(new Uint8Array([0xCE]));   // incomplete
// writer.close();
// // reader yields Uint8Array([0xEF, 0xBF, 0xBD]) (from flush),
// //   then { done: true }.

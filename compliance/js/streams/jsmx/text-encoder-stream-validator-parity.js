// Repo-authored TextEncoderStream validator parity fixture
// (Phase 5b-2). The encoder is stateless: each input chunk
// is validated as UTF-8 with malformed sequences replaced
// by U+FFFD. Valid input passes through unchanged.

// Equivalent WHATWG shape:
//
// const es = new TextEncoderStream();
// const writer = es.writable.getWriter();
// const reader = es.readable.getReader();
// // "ABα" — already well-formed UTF-8
// writer.write(new Uint8Array([0x41, 0x42, 0xCE, 0xB1]));
// // lone continuation byte — invalid
// writer.write(new Uint8Array([0x80]));
// writer.close();
// // chunk 1: Uint8Array([0x41, 0x42, 0xCE, 0xB1]) (passthrough)
// // chunk 2: Uint8Array([0xEF, 0xBF, 0xBD])       (U+FFFD)
// // total bytes collected: 7.

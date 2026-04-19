// Repo-authored TextDecoderStream boundary-split parity
// fixture (Phase 5b-2). Demonstrates the cross-chunk
// trailing-byte buffering: a 4-byte UTF-8 payload arriving
// as 3+1 bytes is reassembled so each output chunk
// contains only complete UTF-8 codepoints.
//
// Note: WHATWG TextDecoderStream produces strings on the
// readable side; the jsmx C variant produces UTF-8 bytes
// (input bytes guaranteed not to split a codepoint across
// an output chunk boundary).

// Equivalent WHATWG shape:
//
// const ds = new TextDecoderStream();
// const writer = ds.writable.getWriter();
// const reader = ds.readable.getReader();
// // CE B1 CE B2 == "αβ"; arrives split as 3 + 1 bytes.
// writer.write(new Uint8Array([0xCE, 0xB1, 0xCE]));
// writer.write(new Uint8Array([0xB2]));
// writer.close();
// const r1 = await reader.read();  // value: [0xCE, 0xB1] = "α"
// const r2 = await reader.read();  // value: [0xCE, 0xB2] = "β"
// const r3 = await reader.read();  // done: true

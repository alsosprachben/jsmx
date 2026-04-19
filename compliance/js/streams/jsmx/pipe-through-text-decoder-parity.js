// Repo-authored pipeThrough TextDecoderStream parity
// fixture (Phase 6-2). The canonical "boundary-correct
// UTF-8 decode of a streaming response body" pattern:
// a ReadableStream of arbitrary byte chunks is piped
// through TextDecoderStream so that no codepoint is split
// across an output chunk boundary, and the resulting
// readable is consumed downstream.

// Equivalent WHATWG shape:
//
// // Source emits 0xCE 0xB1 0xCE then 0xB2 — splits "αβ"
// // mid-codepoint.
// const readable = new ReadableStream({ ... });
// const out = readable.pipeThrough(new TextDecoderStream());
// const reader = out.getReader();
// const r1 = await reader.read();  // value: [0xCE, 0xB1]
// const r2 = await reader.read();  // value: [0xCE, 0xB2]
// const r3 = await reader.read();  // done: true
//
// Asserts the chained-readable identity: the readable
// returned by pipeThrough IS the transform's readable side
// (not a fresh stream), so reads on it observe transformed
// bytes from the source.

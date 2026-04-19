// Repo-authored TransformStream uppercase parity fixture
// (Phase 4-2). Demonstrates in-flight byte transformation:
// each ASCII lowercase byte in the input chunk is mapped
// to its uppercase variant before being enqueued onto the
// readable side.

// Equivalent WHATWG shape:
//
// const ts = new TransformStream({
//   transform(chunk, controller) {
//     const out = new Uint8Array(chunk.length);
//     for (let i = 0; i < chunk.length; i++) {
//       const c = chunk[i];
//       out[i] = (c >= 0x61 && c <= 0x7a) ? c - 0x20 : c;
//     }
//     controller.enqueue(out);
//   },
// });
// // write "abc" → read "ABC" + done

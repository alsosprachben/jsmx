// Repo-authored TransformStream flush-emits parity fixture
// (Phase 4-2). The optional flush() callback runs once on
// writer.close(), giving the transformer a final chance to
// enqueue trailing bytes (e.g. a Z_FINISH marker for
// compression) before the readable side reports EOF. Bytes
// enqueued in flush land between the last data chunk and
// the {done:true} sentinel.

// Equivalent WHATWG shape:
//
// const ts = new TransformStream({
//   transform(chunk, controller) { controller.enqueue(chunk); },
//   flush(controller) {
//     controller.enqueue(new TextEncoder().encode("!END"));
//   },
// });
// // write "data" + close
// // → read "data", "!END", { done: true }

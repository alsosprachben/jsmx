// Repo-authored TransformStream one-to-many parity fixture
// (Phase 4-2). A single writer.write() chunk can produce
// multiple readable chunks via repeated controller.enqueue
// calls inside the transformer. Demonstrates the channel
// FIFO's fan-out behavior.

// Equivalent WHATWG shape:
//
// const ts = new TransformStream({
//   transform(chunk, controller) {
//     const text = new TextDecoder().decode(chunk);
//     for (const part of text.split(",")) {
//       controller.enqueue(new TextEncoder().encode(part));
//     }
//   },
// });
// // write "foo,bar,baz" (single chunk)
// // → read "foo", "bar", "baz", { done: true }

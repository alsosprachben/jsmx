// Repo-authored pipeTo sink-error parity fixture
// (Phase 6-2). When the downstream sink errors mid-pipe,
// the pipe Promise rejects with the sink's reason.

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({
//   start(controller) {
//     controller.enqueue(new TextEncoder().encode("xyz"));
//     controller.close();
//   },
// });
// const writable = new WritableStream({
//   write() { throw new TypeError("nope"); },
// });
// try {
//   await readable.pipeTo(writable);
// } catch (e) {
//   // e is TypeError("nope")
// }

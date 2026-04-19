// Repo-authored pipeTo source-error parity fixture
// (Phase 6-2). When the upstream source errors mid-pipe,
// the pipe Promise rejects with the source's reason.

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({
//   pull(controller) {
//     controller.error(new TypeError("bad"));
//   },
// });
// const writable = new WritableStream({ write() {} });
// try {
//   await readable.pipeTo(writable);
//   // unreachable
// } catch (e) {
//   // e is TypeError("bad")
// }

// Repo-authored WritableStream PENDING -> wake parity
// fixture (Phase 3a-2). When the underlying sink reports
// it cannot accept a chunk yet, the writable-stream pump
// parks rather than busy-looping. The producer's
// notify-on-ready signal re-schedules the pump and the
// previously-queued write resolves.
//
// This is the WritableStream analogue of the ReadableStream
// PENDING/notify behavior validated in test_jsval scenario
// 6 (response-body-parity); surfaced as a compliance fixture
// so a regression that drops the park slot fails loudly.

// Equivalent WHATWG shape (JS):
//
// // Writes return a pending Promise; resolved externally
// // when capacity becomes available.
// let pending;
// const stream = new WritableStream({
//   write(chunk) { return new Promise((r) => { pending = r; }); },
// });
// const writer = stream.getWriter();
// const p = writer.write(new TextEncoder().encode("hi"));
// // p is pending; queue is parked behind the sink.
// pending();   // unblock
// await p;     // now fulfills

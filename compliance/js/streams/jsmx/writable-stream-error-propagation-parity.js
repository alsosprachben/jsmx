// Repo-authored WritableStream error propagation parity
// fixture (Phase 3a-2). When the underlying sink reports
// an error, every queued write rejects with that error,
// the close() promise rejects too, and any subsequent
// writer.write() rejects synchronously from the stored
// error.

// Equivalent WHATWG shape:
//
// const stream = new WritableStream({
//   write() { throw new TypeError("boom"); },
// });
// const writer = stream.getWriter();
// const p1 = writer.write(new TextEncoder().encode("x"));
// const p2 = writer.write(new TextEncoder().encode("y"));
// const pclose = writer.close();
// // p1, p2, pclose all reject with TypeError("boom")
//
// // After the failure is observed, further writes reject
// // immediately with the same stored error:
// await writer.write(new TextEncoder().encode("z"))
//   .catch((e) => e);  // TypeError

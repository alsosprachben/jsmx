// Repo-authored TransformStream error propagation parity
// fixture (Phase 4-2). When the transformer calls
// controller.error(), the stream errors immediately on
// both sides: any pending reader.read() rejects with the
// reason, the writer.write() that triggered the error
// rejects too, and any subsequent writer.write() rejects
// synchronously from the stored error.

// Equivalent WHATWG shape:
//
// const ts = new TransformStream({
//   transform(chunk, controller) {
//     controller.error(new TypeError("bad chunk"));
//   },
// });
// const writer = ts.writable.getWriter();
// const reader = ts.readable.getReader();
// const pwrite = writer.write(new TextEncoder().encode("x"));
// const pread = reader.read();
// // pwrite rejects with TypeError("bad chunk")
// // pread rejects with TypeError("bad chunk")
//
// // Subsequent writes reject too:
// await writer.write(new TextEncoder().encode("y"))
//   .catch((e) => e);  // TypeError

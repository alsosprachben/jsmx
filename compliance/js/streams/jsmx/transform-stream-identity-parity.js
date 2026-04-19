// Repo-authored TransformStream identity parity fixture
// (Phase 4-2). A pass-through transformer just re-enqueues
// each input chunk; bytes round-trip from writer.write()
// on the writable side to reader.read() on the readable
// side, and writer.close() drives the readable to EOF.

// Equivalent WHATWG shape:
//
// const ts = new TransformStream({
//   transform(chunk, controller) { controller.enqueue(chunk); },
// });
// const writer = ts.writable.getWriter();
// const reader = ts.readable.getReader();
// const pw1 = writer.write(new TextEncoder().encode("hello, "));
// const pw2 = writer.write(new TextEncoder().encode("world"));
// const pcl = writer.close();
// const r1 = await reader.read();  // value: "hello, ", done: false
// const r2 = await reader.read();  // value: "world",  done: false
// const r3 = await reader.read();  // done: true

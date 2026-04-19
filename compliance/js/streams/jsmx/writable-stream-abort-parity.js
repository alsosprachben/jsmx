// Repo-authored WritableStream abort parity fixture
// (Phase 3a-2). stream.abort(reason) tears down a
// writable stream: any queued or in-flight write
// rejects with the abort reason, the abort() promise
// itself fulfills with undefined, the underlying sink's
// close callback runs (giving the sink a chance to
// release resources), and any subsequent writer.write()
// rejects synchronously from the stored error.

// Equivalent WHATWG shape:
//
// const sink = { closed: false };
// const stream = new WritableStream({
//   write(chunk) { return new Promise(() => {}); /* parks */ },
//   close() { sink.closed = true; },
// });
// const writer = stream.getWriter();
// const p1 = writer.write(new TextEncoder().encode("x")); // pending
// const reason = new TypeError("nope");
// const pAbort = stream.abort(reason);
// // p1 rejects with reason
// // pAbort fulfills with undefined
// // sink.closed === true
// // writer.write(...) after abort rejects with reason

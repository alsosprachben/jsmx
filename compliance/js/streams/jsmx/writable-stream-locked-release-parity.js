// Repo-authored WritableStream lock parity fixture
// (Phase 3a-2). A WritableStream supports a single
// active default writer at a time: getWriter() on a
// locked stream throws, releaseLock() clears the lock,
// and writer methods called after releaseLock() reject
// (the released writer is detached from its stream).

// Equivalent WHATWG shape:
//
// const stream = new WritableStream({ write() {} });
// const w1 = stream.getWriter();
// // stream.locked === true
// try { stream.getWriter(); } catch (e) { /* TypeError */ }
// w1.releaseLock();
// // stream.locked === false
// const w2 = stream.getWriter(); // succeeds
// // w1.write(...) rejects (writer no longer attached)

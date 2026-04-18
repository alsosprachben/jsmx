// Repo-authored ReadableStream reader-lock parity fixture.
//
// getReader() locks the stream; a second getReader() on the
// same stream throws TypeError. releaseLock() unlocks and
// lets a fresh reader acquire.

// const stream = /* some ReadableStream over "abc" */;
// stream.locked === false
// const r1 = stream.getReader();
// stream.locked === true
//
// // Second getReader on locked stream throws:
// try { stream.getReader(); } catch (e) {
//   // e instanceof TypeError
// }
//
// r1.releaseLock();
// stream.locked === false
// const r2 = stream.getReader();  // succeeds

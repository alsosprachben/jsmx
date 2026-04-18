// Repo-authored ReadableStream cancel() parity fixture.
//
// cancel() closes the stream, drops buffered source bytes,
// and resolves its returned Promise with undefined. All
// subsequent reader.read() calls resolve with
// { value: undefined, done: true } — even though the
// source had unread bytes.

// const stream = /* ReadableStream over "abcdef" */;
// const reader = stream.getReader();
// const cancelPromise = stream.cancel();
// await cancelPromise;  // undefined
//
// const { value, done: d1 } = await reader.read();
// // value === undefined
// // d1 === true (even though "abcdef" was not drained)
//
// const { done: d2 } = await reader.read();
// // d2 === true (still closed)

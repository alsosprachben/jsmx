// Repo-authored pipeTo locked-stream parity fixture
// (Phase 6-2). pipeTo on a ReadableStream that already
// has a default reader fails synchronously: WHATWG raises
// TypeError; the jsmx C variant returns < 0 with errno
// EBUSY before scheduling any microtask. No promise is
// returned; the writable side is never claimed.

// Equivalent WHATWG shape:
//
// const readable = new ReadableStream({ start(c) { c.close(); } });
// const writable = new WritableStream({ write() {} });
// const heldReader = readable.getReader();   // pre-lock
// try {
//   await readable.pipeTo(writable);
// } catch (e) {
//   // e is TypeError: stream is locked
// }

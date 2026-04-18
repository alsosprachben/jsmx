// Repo-authored ReadableStream from-bytes parity fixture.
//
// A stream created from an in-memory byte payload yields a
// single Uint8Array chunk containing all source bytes, and
// then reports { done: true } on every subsequent read.

// Equivalent WHATWG shape — construction via constructor is
// deferred to a later phase, so this documents the idiomatic
// JS surface without driving C assertions:
//
// const stream = new ReadableStream({
//   start(controller) {
//     controller.enqueue(new TextEncoder().encode("hello"));
//     controller.close();
//   },
// });
// const reader = stream.getReader();
// const { value, done: d1 } = await reader.read();
// // value is a Uint8Array with bytes [0x68, 0x65, 0x6c, 0x6c, 0x6f]
// // d1 === false
// const { done: d2 } = await reader.read();
// // d2 === true
// const { done: d3 } = await reader.read();
// // d3 === true (still done after close)

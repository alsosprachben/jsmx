// Repo-authored Response.body parity fixture.
//
// Response.body returns a ReadableStream; draining it via a
// default reader yields the body bytes. Accessing .body flips
// bodyUsed in this build (Phase 1B simplification), so
// subsequent consumer calls (text/json/arrayBuffer/bytes)
// reject with TypeError.

// const response = new Response("world");
// response.bodyUsed === false
// const stream = response.body;
// // stream instanceof ReadableStream
// response.bodyUsed === true (Phase 1B: .body consumes)
//
// const reader = stream.getReader();
// const { value, done: d1 } = await reader.read();
// // value equals new Uint8Array([0x77, 0x6f, 0x72, 0x6c, 0x64])
// // d1 === false
// const { done: d2 } = await reader.read();
// // d2 === true
//
// // Consumer method on already-used body rejects:
// await response.text();  // throws TypeError("body already used")

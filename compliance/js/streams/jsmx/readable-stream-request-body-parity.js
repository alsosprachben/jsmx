// Repo-authored Request.body parity fixture — symmetric to
// Response.body. new Request(url, { method, body }).body
// returns a ReadableStream; draining via a default reader
// yields the body bytes. Accessing .body flips bodyUsed
// (Phase-1D simplification matching the Response side), so
// subsequent text/json/arrayBuffer/bytes consumers on the
// same Request reject with TypeError.

// const req = new Request("https://ex.com", {
//   method: "POST",
//   body: "world",
// });
// req.bodyUsed === false
// const stream = req.body;
// // stream instanceof ReadableStream
// req.bodyUsed === true (Phase 1D: .body consumes)
//
// const reader = stream.getReader();
// const { value, done: d1 } = await reader.read();
// // value equals new Uint8Array([0x77, 0x6f, 0x72, 0x6c, 0x64])
// // d1 === false
// const { done: d2 } = await reader.read();
// // d2 === true
//
// // Consumer method on already-used body rejects:
// await req.text();  // throws TypeError("body already used")

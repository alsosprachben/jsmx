// Repo-authored Response.arrayBuffer() parity fixture.
//
// Response.arrayBuffer() returns a Promise that resolves to an
// ArrayBuffer holding the body's UTF-8 bytes. jsmx's current
// transports materialize bodies synchronously in-region, so the
// Promise is already fulfilled at the call site.
// Response.bodyUsed flips from false to true after consumption.

const r = new Response("hello");
// r.bodyUsed === false
// const buf = await r.arrayBuffer();
// new Uint8Array(buf) matches UTF-8 bytes for "hello" [0x68, 0x65, 0x6c, 0x6c, 0x6f]
// r.bodyUsed === true

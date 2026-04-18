// Repo-authored Response.bytes() parity fixture.
//
// Response.bytes() is a newer Fetch spec addition that returns
// a Promise resolving to a Uint8Array directly (vs. arrayBuffer
// which returns an ArrayBuffer). In jsmx, the in-memory
// transport resolves synchronously.

const r = new Response("café");
// const bytes = await r.bytes();
// bytes instanceof Uint8Array === true
// bytes matches UTF-8 of "café": [0x63, 0x61, 0x66, 0xc3, 0xa9]

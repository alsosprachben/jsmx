// Repo-authored Request.arrayBuffer() parity fixture.
//
// Mirrors the Response.arrayBuffer() fixture: an in-memory
// Request body is consumed synchronously via arrayBuffer(),
// and bodyUsed flips across the call.

const req = new Request("https://ex.com", {
	method: "POST",
	body: "hello",
});

// req.bodyUsed === false (pre-consume)
// const ab = await req.arrayBuffer();
// ab.byteLength === 5
// new Uint8Array(ab) equals [0x68, 0x65, 0x6c, 0x6c, 0x6f]
// req.bodyUsed === true (post-consume)

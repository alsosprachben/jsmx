// Repo-authored Request.bytes() parity fixture.
//
// Mirrors the Response.bytes() fixture: a multi-byte body
// "café" (5 UTF-8 bytes) materializes as a Uint8Array whose
// backing bytes equal the UTF-8 encoding.

const req = new Request("https://ex.com", {
	method: "POST",
	body: "café",
});

// const u8 = await req.bytes();
// u8 instanceof Uint8Array
// u8.length === 5
// u8 equals [0x63, 0x61, 0x66, 0xc3, 0xa9]
// req.bodyUsed === true

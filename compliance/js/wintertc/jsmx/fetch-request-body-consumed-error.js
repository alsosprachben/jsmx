// Repo-authored Request body already-used error fixture.
//
// Mirrors the Response body-consumed-error fixture: the
// first body-consumer call fulfills and sets bodyUsed; any
// subsequent consumer call (same kind or cross-kind) returns
// a Promise rejected with TypeError "body already used".

const req = new Request("https://ex.com", {
	method: "POST",
	body: "hello",
});

// await req.text() → "hello"; req.bodyUsed === true.
// req.text() now rejects with TypeError("body already used").
// req.json() on the same Request also rejects with the same
// TypeError.
// bodyUsed stays true across subsequent inspection.

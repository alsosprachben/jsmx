// Repo-authored Response body already-used error fixture.
//
// Once a Response's body is consumed (via text / json /
// arrayBuffer / bytes), a second consumer call returns a
// Promise rejected with TypeError("body already used"). This
// holds across consumers: text() then json() still rejects.

const r = new Response("hello");
const first  = await r.text();
// r.bodyUsed === true
// first === "hello"

// Second consume on same Response: rejects.
let threw = false;
try { await r.text(); } catch (e) { threw = true; }
// threw === true
// Error is TypeError with message "body already used".

// Cross-consumer mismatch also rejects.
let threw_cross = false;
try { await r.json(); } catch (e) { threw_cross = true; }
// threw_cross === true

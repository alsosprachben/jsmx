// Repo-authored WHATWG Fetch Response parity fixture.
const r1 = new Response("hello", { status: 201, statusText: "Created" });
const r2 = Response.json({ ok: true }, { status: 200 });
const r3 = Response.error();

[
  r1.status,                                  // 201
  r1.ok,                                      // true
  r1.text(),                                  // Promise<"hello">
  r2.headers.get("content-type"),             // "application/json"
  r2.json(),                                  // Promise<{ok:true}>
  r3.type,                                    // "error"
  r3.status,                                  // 0
];

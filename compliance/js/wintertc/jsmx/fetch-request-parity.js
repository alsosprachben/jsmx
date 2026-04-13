// Repo-authored WHATWG Fetch Request parity fixture.
const req = new Request("https://example.com/api", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: '{"x":1}',
});

[
  req.method,                          // "POST"
  req.url,                             // "https://example.com/api"
  req.headers.get("content-type"),     // "application/json"
  req.bodyUsed,                        // false
  req.text(),                          // Promise<'{"x":1}'>
];

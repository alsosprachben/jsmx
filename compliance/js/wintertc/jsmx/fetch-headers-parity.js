// Repo-authored WHATWG Fetch Headers parity fixture.
const h = new Headers();
h.append("Content-Type", "application/json");
h.append("X-Custom", "  first  ");
h.append("X-Custom", "second");

[
  h.has("content-type"),           // true
  h.get("CONTENT-TYPE"),           // "application/json"
  h.get("x-custom"),               // "first, second" — spec combine
  h.get("missing"),                // null
];

// Repo-authored WHATWG Fetch Request rejection fixture.
const captured = [];

try {
  new Request("https://example.com", { method: "CONNECT" });
} catch (e) {
  captured.push(e.name);
}

try {
  new Request("https://example.com", { method: "TRACE" });
} catch (e) {
  captured.push(e.name);
}

try {
  new Request("https://example.com", { method: "TRACK" });
} catch (e) {
  captured.push(e.name);
}

captured;

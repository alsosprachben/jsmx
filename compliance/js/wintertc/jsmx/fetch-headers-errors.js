// Repo-authored WHATWG Fetch Headers rejection fixture.
const h = new Headers();
const captured = [];

try {
  h.append("bad name", "v");
} catch (e) {
  captured.push(e.name);
}

try {
  h.append("X-Ok", "has\nnewline");
} catch (e) {
  captured.push(e.name);
}

try {
  h.append("", "value");
} catch (e) {
  captured.push(e.name);
}

captured;

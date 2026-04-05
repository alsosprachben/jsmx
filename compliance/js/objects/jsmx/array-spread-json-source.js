// Repo-authored idiomatic lowering fixture for fresh array spread from a
// JSON-backed dense array.

const src = JSON.parse('[1,2]');
const clone = [...src];

clone.push(3);

if (JSON.stringify(src) !== "[1,2]") {
  throw new Error("expected JSON-backed array spread source to remain unchanged");
}

if (JSON.stringify(clone) !== "[1,2,3]") {
  throw new Error("expected JSON-backed array spread clone to append independently");
}

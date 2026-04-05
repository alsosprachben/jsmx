// Repo-authored idiomatic lowering fixture for fresh dense-array cloning from
// a JSON-backed source into a new native array.

const src = JSON.parse("[1,2]");
const clone = [...src];

clone[0] = 7;

if (JSON.stringify(src) !== "[1,2]") {
  throw new Error("expected JSON-backed source to remain unchanged after fresh dense-array clone mutation");
}

if (JSON.stringify(clone) !== "[7,2]") {
  throw new Error("expected fresh clone from JSON-backed array source to preserve dense order");
}

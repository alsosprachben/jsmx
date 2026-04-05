// Repo-authored idiomatic lowering fixture for fresh shallow own-property
// cloning from a JSON-backed source into a new native object.

const src = JSON.parse('{"z":1,"a":2}');
const clone = Object.assign({}, src);

clone.z = 7;

if (JSON.stringify(src) !== '{"z":1,"a":2}') {
  throw new Error("expected JSON-backed source to remain unchanged after fresh clone mutation");
}

if (JSON.stringify(clone) !== '{"z":7,"a":2}') {
  throw new Error("expected fresh clone from JSON source to preserve parsed key order");
}

// Repo-authored idiomatic lowering fixture for fresh object spread from a
// JSON-backed source object.

const src = JSON.parse('{"z":1,"a":2}');
const clone = { ...src };

if (JSON.stringify(clone) !== '{"z":1,"a":2}') {
  throw new Error("expected JSON-backed object spread clone to preserve parsed source order");
}

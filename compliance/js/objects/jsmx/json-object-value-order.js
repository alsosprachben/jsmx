// Repo-authored idiomatic lowering fixture for parsed-source value order on a
// JSON-backed object.

const obj = JSON.parse('{"z":1,"a":2,"m":3}');

if (JSON.stringify(Object.values(obj)) !== '[1,2,3]') {
  throw new Error("expected JSON-backed value access to preserve parsed source order");
}

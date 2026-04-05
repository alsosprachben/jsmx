// Repo-authored idiomatic lowering fixture for parsed-source key order on a
// JSON-backed object.

const obj = JSON.parse('{"z":1,"a":2,"m":3}');

if (JSON.stringify(Object.keys(obj)) !== '["z","a","m"]') {
  throw new Error("expected JSON-backed key access to preserve parsed source order");
}

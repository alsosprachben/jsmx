// Repo-authored idiomatic lowering fixture for shallow own-property copy from a
// JSON-backed source object into a native destination.

const dst = { keep: true };
const src = JSON.parse('{"z":1,"a":2}');

Object.assign(dst, src);

if (JSON.stringify(dst) !== '{"keep":true,"z":1,"a":2}') {
  throw new Error("expected JSON-backed own-property copy to preserve parsed source order");
}

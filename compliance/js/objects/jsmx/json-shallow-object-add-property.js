// Repo-authored idiomatic lowering fixture for shallow object promotion with
// translator-planned property capacity.

const root = JSON.parse('{"profile":{"name":"Ada"},"scores":[1,2],"status":"ok"}');

root.ready = true;

if (JSON.stringify(root) !== '{"profile":{"name":"Ada"},"scores":[1,2],"status":"ok","ready":true}') {
  throw new Error("expected shallow-promoted root writes to preserve token-backed children and emit the new property");
}

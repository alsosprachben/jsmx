// Repo-authored idiomatic lowering fixture for shallow child-array promotion
// with translator-planned dense capacity.

const root = JSON.parse('{"profile":{"name":"Ada"},"scores":[1,2],"status":"ok"}');

root.ready = true;
root.scores.push(3);
root.scores.length = 4;

if (root.scores[3] !== undefined) {
  throw new Error("expected dense length growth to create an undefined trailing slot before it is filled");
}

root.scores[3] = 4;

if (JSON.stringify(root) !== '{"profile":{"name":"Ada"},"scores":[1,2,3,4],"status":"ok","ready":true}') {
  throw new Error("expected shallow-promoted child array writes to emit the updated JSON document");
}

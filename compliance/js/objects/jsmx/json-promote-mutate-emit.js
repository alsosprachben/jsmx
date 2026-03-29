// Repo-authored idiomatic lowering fixture for JSON-backed reads, explicit
// promotion on mutation, native writes, and JSON emission.

const root = JSON.parse('{"profile":{"name":"Ada","active":true},"scores":[1,2,3],"note":"hi"}');

if (root.profile.name !== "Ada") {
  throw new Error("expected JSON-backed object read to preserve nested string values");
}

if (root.scores[1] !== 2) {
  throw new Error("expected JSON-backed array read to preserve numeric values");
}

root.profile.active = false;
root.scores[0] = 7;
root.note = "updated";

if (JSON.stringify(root) !== '{"profile":{"name":"Ada","active":false},"scores":[7,2,3],"note":"updated"}') {
  throw new Error("expected promoted native writes to emit the updated JSON document");
}

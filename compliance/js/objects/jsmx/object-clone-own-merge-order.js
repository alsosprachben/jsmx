// Repo-authored idiomatic lowering fixture for clone-then-merge behavior in
// Object.assign-style left-to-right source order.

const first = { keep: true, drop: 7, items: [] };
const second = JSON.parse('{"drop":9,"tail":10}');
const merged = Object.assign({}, first, second);

if (JSON.stringify(merged) !== '{"keep":true,"drop":9,"items":[],"tail":10}') {
  throw new Error("expected clone-then-merge lowering to preserve overwrite and append order");
}

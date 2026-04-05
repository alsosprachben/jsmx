// Repo-authored idiomatic lowering fixture for object spread merge order with
// later sources overwriting earlier keys in place.

const first = { keep: true, drop: 7, items: [] };
const second = JSON.parse('{"drop":9,"tail":10}');
const merged = { ...first, ...second };

if (JSON.stringify(merged) !== '{"keep":true,"drop":9,"items":[],"tail":10}') {
  throw new Error("expected object spread merge to preserve overwrite and append order");
}

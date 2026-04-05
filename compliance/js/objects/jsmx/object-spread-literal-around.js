// Repo-authored idiomatic lowering fixture for literal properties around an
// object spread segment, including a literal overwrite after spread.

const src = JSON.parse('{"mid":7,"tail":8}');
const out = { head: 1, ...src, tail: 2 };

if (JSON.stringify(out) !== '{"head":1,"mid":7,"tail":2}') {
  throw new Error("expected literal properties around object spread to preserve key order");
}

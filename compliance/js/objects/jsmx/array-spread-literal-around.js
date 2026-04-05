// Repo-authored idiomatic lowering fixture for literal elements around an
// array spread segment.

const src = JSON.parse('[1,2]');
const out = [0, ...src, 9];

if (JSON.stringify(out) !== "[0,1,2,9]") {
  throw new Error("expected literal elements around array spread to preserve order");
}

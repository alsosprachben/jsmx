// Repo-authored idiomatic lowering fixture for array spread concatenation
// across multiple dense sources.

const first = JSON.parse('[1,2]');
const second = JSON.parse('[3,4]');
const merged = [...first, ...second];

if (JSON.stringify(merged) !== "[1,2,3,4]") {
  throw new Error("expected array spread concatenation to preserve source order");
}

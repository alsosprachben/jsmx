// Repo-authored idiomatic lowering fixture for fresh array spread from a
// native dense array.

const src = [4, 5];
const clone = [...src];

clone[0] = 9;

if (JSON.stringify(src) !== "[4,5]") {
  throw new Error("expected native array spread source to remain unchanged");
}

if (JSON.stringify(clone) !== "[9,5]") {
  throw new Error("expected native array spread clone to support independent overwrite");
}

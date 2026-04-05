// Repo-authored idiomatic lowering fixture for fresh dense-array cloning from
// one native array into a new native array.

const src = [1, 2];
const clone = [...src];

clone.push(3);

if (JSON.stringify(src) !== "[1,2]") {
  throw new Error("expected native source to remain unchanged after fresh dense-array clone mutation");
}

if (JSON.stringify(clone) !== "[1,2,3]") {
  throw new Error("expected fresh native dense-array clone to preserve element order and allow independent append");
}

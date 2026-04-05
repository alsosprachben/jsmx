// Repo-authored idiomatic lowering fixture for clone-then-mutate behavior on
// a fresh dense array.

const src = [4, 5];
const clone = [...src];

clone.shift();
clone.unshift(9);

if (JSON.stringify(src) !== "[4,5]") {
  throw new Error("expected original dense array to remain unchanged after clone front mutation");
}

if (JSON.stringify(clone) !== "[9,5]") {
  throw new Error("expected cloned dense array to support independent front mutation");
}

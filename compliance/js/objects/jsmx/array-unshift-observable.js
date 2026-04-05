// Repo-authored idiomatic lowering fixture for dense native array unshift.

const arr = [1];

arr.unshift(9);
arr.unshift(7);

if (JSON.stringify(arr) !== "[7,9,1]") {
  throw new Error("expected unshift to compact the existing dense prefix to the right");
}

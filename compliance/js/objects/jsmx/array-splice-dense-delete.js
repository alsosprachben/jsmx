// Repo-authored idiomatic lowering fixture for delete-only dense-array splice.

const arr = [1, 2, 3, 4];
const removed = arr.splice(1, 2);

if (JSON.stringify(arr) !== "[1,4]") {
  throw new Error("expected dense delete-only splice to compact the remaining suffix");
}

if (JSON.stringify(removed) !== "[2,3]") {
  throw new Error("expected dense delete-only splice to return the removed middle segment");
}

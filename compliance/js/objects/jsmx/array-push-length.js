// Repo-authored idiomatic lowering fixture for dense native array push and
// explicit length writes.

const arr = [1];

arr.push(2);
arr.length = 4;

if (arr.length !== 4) {
  throw new Error("expected explicit length growth to update the logical length");
}

if (arr[2] !== undefined || arr[3] !== undefined) {
  throw new Error("expected grown dense slots to read back as undefined");
}

arr.length = 2;

if (arr.length !== 2) {
  throw new Error("expected explicit length shrink to truncate the logical length");
}

if (JSON.stringify(arr) !== "[1,2]") {
  throw new Error("expected truncation to be reflected in emitted JSON");
}

// Repo-authored idiomatic lowering fixture for dense native array index
// overwrites.

const arr = [1, 2, 3];

arr[1] = 9;
arr[2] = 4;

if (arr.length !== 3) {
  throw new Error("expected dense index overwrites to preserve logical length");
}

if (arr[1] !== 9 || arr[2] !== 4) {
  throw new Error("expected dense index overwrites to replace the stored values");
}

if (JSON.stringify(arr) !== "[1,9,4]") {
  throw new Error("expected emitted JSON to reflect dense index overwrites");
}

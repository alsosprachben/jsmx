// Repo-authored idiomatic lowering fixture for empty shift and undefined slot
// shifts after dense length growth.

const arr = [];

if (arr.shift() !== undefined) {
  throw new Error("expected empty shift to return undefined");
}

arr.length = 2;

if (arr.shift() !== undefined) {
  throw new Error("expected first undefined shift to return undefined");
}

if (arr.shift() !== undefined) {
  throw new Error("expected second undefined shift to return undefined");
}

if (arr.length !== 0 || JSON.stringify(arr) !== "[]") {
  throw new Error("expected repeated undefined shifts to leave an empty dense array");
}

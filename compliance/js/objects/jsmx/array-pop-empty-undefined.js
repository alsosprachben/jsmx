// Repo-authored idiomatic lowering fixture for empty pop and undefined tail pop.

const arr = [];

if (arr.pop() !== undefined) {
  throw new Error("expected empty pop to return undefined");
}

arr.length = 2;

if (arr.pop() !== undefined) {
  throw new Error("expected grown undefined tail pop to return undefined");
}

if (arr.pop() !== undefined) {
  throw new Error("expected final undefined pop to return undefined");
}

if (arr.length !== 0 || JSON.stringify(arr) !== "[]") {
  throw new Error("expected repeated undefined pops to leave an empty dense array");
}

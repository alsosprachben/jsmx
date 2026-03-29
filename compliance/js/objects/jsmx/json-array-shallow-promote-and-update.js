// Repo-authored idiomatic lowering fixture for a parsed JSON-backed array that
// is shallow-promoted with explicit final capacity before mutation.

const arr = JSON.parse("[1,2]");

if (arr[1] !== 2) {
  throw new Error("expected JSON-backed reads to see the original parsed values");
}

arr[0] = 7;
arr.push(3);

if (JSON.stringify(arr) !== "[7,2,3]") {
  throw new Error("expected shallow-promoted dense array updates to emit the final JSON");
}

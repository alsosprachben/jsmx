// Repo-authored idiomatic lowering fixture for dense native array length
// growth, writes into grown slots, and truncating shrink.

const arr = [1, 2];

arr.length = 4;

if (arr[2] !== undefined || arr[3] !== undefined) {
  throw new Error("expected dense length growth to expose undefined trailing slots");
}

arr[2] = 7;
arr[3] = 8;
arr.length = 2;

if (arr.length !== 2) {
  throw new Error("expected dense length shrink to truncate the logical array");
}

if (arr[2] !== undefined) {
  throw new Error("expected truncated dense slots to stop participating in reads");
}

if (JSON.stringify(arr) !== "[1,2]") {
  throw new Error("expected emitted JSON to reflect only the final dense prefix");
}

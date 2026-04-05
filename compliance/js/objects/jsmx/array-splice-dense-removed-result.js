// Repo-authored idiomatic lowering fixture for observing the removed-result
// array from a dense splice with delete-count clamping.

const arr = [1, 2];
const removed = arr.splice(1, 5);

if (JSON.stringify(arr) !== "[1]") {
  throw new Error("expected dense splice to clamp deleteCount to the visible tail");
}

if (JSON.stringify(removed) !== "[2]") {
  throw new Error("expected dense splice to return the clamped removed suffix");
}

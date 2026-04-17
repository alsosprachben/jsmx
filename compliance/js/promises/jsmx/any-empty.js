// Repo-authored Promise.any compliance fixture.
//
// Empty input: Promise.any rejects synchronously with an
// AggregateError whose errors array is empty. Exercises the
// n == 0 fast path inside jsval_promise_any.

const result = Promise.any([]);
// result rejects with AggregateError([], "All promises were rejected")

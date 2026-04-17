// Repo-authored Promise.allSettled compliance fixture.
//
// Empty input: Promise.allSettled resolves synchronously with
// an empty array. Exercises the n == 0 fast path inside
// jsval_promise_all_settled.

const result = Promise.allSettled([]);
// result fulfills with []

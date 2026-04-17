// Repo-authored Promise.all compliance fixture.
//
// When all input promises are already fulfilled, Promise.all
// resolves synchronously with their results in input order.
// Exercises jsmx's fast-path inside jsval_promise_all (walk all
// inputs, collect results, resolve output before linking the
// coordinator).

const result = Promise.all([
  Promise.resolve(10),
  Promise.resolve(20),
  Promise.resolve(30),
]);
// result fulfills with [10, 20, 30]

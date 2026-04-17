// Repo-authored Promise.any compliance fixture.
//
// When any input promise is already fulfilled, Promise.any
// resolves synchronously with that value (first-fulfilled wins
// in input order). Exercises jsmx's fast-path inside
// jsval_promise_any.

const result = Promise.any([
  Promise.reject(1),
  Promise.resolve(42),
  Promise.resolve(7),
]);
// result fulfills with 42

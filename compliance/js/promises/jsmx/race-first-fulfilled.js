// Repo-authored Promise.race compliance fixture.
//
// When the first input is already fulfilled, Promise.race
// resolves synchronously with that value. Exercises jsmx's
// fast-path inside jsval_promise_race: walking the input array
// in order finds the first non-pending and propagates its
// state to the output.

const result = Promise.race([
  Promise.resolve(42),
  Promise.resolve(7),
]);
// result fulfills with 42

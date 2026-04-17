// Repo-authored Promise.race compliance fixture.
//
// When the first input is already rejected, Promise.race
// rejects synchronously with that reason. Exercises jsmx's
// fast-path inside jsval_promise_race walking inputs in order:
// the first non-pending (rejected) one propagates its reason
// to the output.

const result = Promise.race([
  Promise.reject(99),
  Promise.resolve(7),
]);
// result rejects with 99

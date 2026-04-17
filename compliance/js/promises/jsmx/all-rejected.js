// Repo-authored Promise.all compliance fixture.
//
// When any input promise is rejected, Promise.all rejects with
// that rejection reason (first-rejection-wins). Exercises jsmx's
// fast-path rejection short-circuit: walking inputs encounters
// the rejected one, calls jsval_promise_reject on the aggregate
// with its reason, and returns without linking a coordinator.

const result = Promise.all([
  Promise.resolve(1),
  Promise.reject(99),
  Promise.resolve(3),
]);
// result rejects with 99

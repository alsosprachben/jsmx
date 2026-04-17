// Repo-authored Promise.all compliance fixture.
//
// When some inputs are already fulfilled but at least one is
// still pending at Promise.all's call site, the aggregate stays
// pending until the last input settles. Results preserve input
// order regardless of completion order. Exercises jsmx's slow
// path: jsval_promise_all allocates a coordinator, links it
// into the region's promise_all_head list, and
// jsval_microtask_drain's scan phase settles it after the
// pending input resolves.

const pending = new Promise((resolve) => { /* will be resolved below */ });
const result = Promise.all([
  pending,
  Promise.resolve(200),
  Promise.resolve(300),
]);
// result is pending at call site.
// ...later: resolve(pending, 100); drain microtasks.
// Then result fulfills with [100, 200, 300].

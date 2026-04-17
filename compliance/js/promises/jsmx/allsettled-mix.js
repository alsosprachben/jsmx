// Repo-authored Promise.allSettled compliance fixture.
//
// Mix of fulfilled and rejected inputs, pre-settled: allSettled
// fulfills (never rejects) with an array of result descriptors:
//   { status: "fulfilled", value: X }
//   { status: "rejected",  reason: Y }
// in input order. Exercises jsmx's fast-path: all inputs
// already settled, result objects built synchronously.

const result = Promise.allSettled([
  Promise.resolve(1),
  Promise.reject(99),
  Promise.resolve(3),
]);
// result fulfills with
//   [ { status: "fulfilled", value: 1 },
//     { status: "rejected",  reason: 99 },
//     { status: "fulfilled", value: 3 } ]

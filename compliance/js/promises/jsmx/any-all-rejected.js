// Repo-authored Promise.any compliance fixture.
//
// When every input promise is rejected, Promise.any rejects
// with an AggregateError whose `errors` array contains each
// input's rejection reason in input order, and whose `message`
// is "All promises were rejected".

const result = Promise.any([
  Promise.reject(1),
  Promise.reject(2),
  Promise.reject(3),
]);
// result rejects with AggregateError:
//   name === "AggregateError"
//   message === "All promises were rejected"
//   errors === [1, 2, 3]

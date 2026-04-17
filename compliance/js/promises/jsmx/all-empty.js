// Repo-authored Promise.all compliance fixture.
//
// ECMAScript says Promise.all over an empty iterable must fulfill
// synchronously with an empty array. jsmx's jsval_promise_all
// exposes the n == 0 shortcut via NULL inputs + n == 0.

const result = Promise.all([]);
// result is a fulfilled Promise; its value is []

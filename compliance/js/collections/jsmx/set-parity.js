// Repo-authored Set parity fixture.
//
// Set membership uses sameValueZero: NaN equals NaN; -0 equals
// +0. jsmx's Set is capacity-bounded (add past cap returns
// ENOBUFS), with jsval_set_clone as the grow operation.

const s = new Set();
// typeof s === "object"; s.size === 0

s.add("a");
// s.has("a") === true
// s.size === 1

s.add("a");  // duplicate — no-op
// s.size === 1

s.add(NaN);
s.add(NaN);  // deduped
// s.has(NaN) === true
// s.size === 2

s.add(-0);
// s.has(0) === true   (sameValueZero)

// Delete returns true on first delete, false on second.
const first_delete  = s.delete("a");     // true
const second_delete = s.delete("a");     // false

s.clear();
// s.size === 0

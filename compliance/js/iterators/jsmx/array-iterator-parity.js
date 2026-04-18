// Repo-authored array iterator parity fixture.
//
// Array iteration supports four selectors: DEFAULT (values),
// KEYS, VALUES, ENTRIES. jsmx's jsval_iterator_next handles
// DEFAULT/KEYS/VALUES; ENTRIES iterators must use
// jsval_iterator_next_entry (the generic _next returns -1).

const arr = [10, 20, 30];

// DEFAULT: yields values 10, 20, 30, done.
// for (const v of arr) — v takes 10, 20, 30.

// KEYS: indices 0, 1, 2.
// for (const k of arr.keys()) — k takes 0, 1, 2.

// VALUES: same as DEFAULT for arrays.
// for (const v of arr.values()) — v takes 10, 20, 30.

// ENTRIES: (index, value) pairs.
// for (const [k, v] of arr.entries()) — k/v pairs (0,10), (1,20), (2,30).

// Empty array iteration: zero steps before done.
const empty = [];

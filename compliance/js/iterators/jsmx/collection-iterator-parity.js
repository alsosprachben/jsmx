// Repo-authored Set + Map iterator parity fixture.
//
// Set iteration yields values in insertion order. Map iteration
// supports DEFAULT (= ENTRIES), KEYS, VALUES, ENTRIES; all
// preserve insertion order.

const s = new Set(["a", "b", "c"]);
// for (const v of s) — v takes "a", "b", "c"

const m = new Map([["x", 1], ["y", 2]]);
// for (const [k, v] of m) — ("x", 1), ("y", 2)
// for (const k of m.keys())  — "x", "y"
// for (const v of m.values())— 1, 2
// for (const [k, v] of m.entries()) — ("x", 1), ("y", 2)

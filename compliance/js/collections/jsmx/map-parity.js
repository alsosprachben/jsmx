// Repo-authored Map parity fixture.
//
// Map set/get/has/delete plus insertion-order preservation
// exposed via key_at / value_at index accessors (jsmx's
// iteration primitives).

const m = new Map();
// typeof m === "object"; m.size === 0

m.set("a", 1);
// m.get("a") === 1
// m.size === 1

m.set("a", 2);  // overwrite
// m.get("a") === 2
// m.size === 1  (no duplicate slot)

m.has("a");      // true
m.has("missing"); // false

m.set("b", 10);
m.set("c", 20);
// m.size === 3

// Insertion order preserved: iteration yields a, b, c.

const removed = m.delete("a");  // true
// m.size === 2

m.clear();
// m.size === 0

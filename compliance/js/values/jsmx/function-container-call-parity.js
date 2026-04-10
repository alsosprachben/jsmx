// Repo-authored idiomatic lowering fixture for native function values stored in native containers.

const fn = function echo(value) {
  return value;
};
const box = { fn };
const arr = [fn];
const set = new Set();
const map = new Map();

set.add(fn);
map.set(fn, "stored");

if (box.fn !== fn || arr[0] !== fn) {
  throw new Error("expected function identity to survive object and array storage");
}

if (!set.has(fn) || map.get(fn) !== "stored") {
  throw new Error("expected function identity to survive Set and Map storage");
}

if (box.fn("ok") !== "ok" || arr[0](7) !== 7) {
  throw new Error("expected retrieved function values to remain callable");
}

// Repo-authored idiomatic lowering fixture for ordered native object entries
// built from explicit jsval key/value helper calls.

const obj = { z: 1, a: 2, m: 3 };

obj.a = 9;

if (JSON.stringify(Object.entries(obj)) !== '[["z",1],["a",9],["m",3]]') {
  throw new Error("expected overwrite to preserve native entry order");
}

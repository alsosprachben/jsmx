// Repo-authored idiomatic lowering fixture for ordered native object value
// access through explicit jsval helper calls.

const obj = { z: 1, a: 2, m: 3 };

obj.a = 9;

if (JSON.stringify(Object.values(obj)) !== '[1,9,3]') {
  throw new Error("expected overwrite to preserve native value order");
}

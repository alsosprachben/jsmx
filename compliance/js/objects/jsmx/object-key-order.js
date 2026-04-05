// Repo-authored idiomatic lowering fixture for ordered native object key
// access through explicit jsval helper calls.

const obj = { z: 1, a: 2, m: 3 };

obj.a = 9;

if (JSON.stringify(Object.keys(obj)) !== '["z","a","m"]') {
  throw new Error("expected overwrite to preserve native key order");
}

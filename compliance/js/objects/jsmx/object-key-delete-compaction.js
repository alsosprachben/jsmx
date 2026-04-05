// Repo-authored idiomatic lowering fixture for delete-compacted native object
// key access through explicit jsval helper calls.

const obj = { keep: true, drop: 7, items: [] };

delete obj.drop;

if (JSON.stringify(Object.keys(obj)) !== '["keep","items"]') {
  throw new Error("expected delete to compact later native keys");
}

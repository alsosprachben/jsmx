// Repo-authored idiomatic lowering fixture for ordered native object value
// access after delete compacts later properties.

const obj = { keep: true, drop: 7, items: [] };

delete obj.drop;

if (JSON.stringify(Object.values(obj)) !== '[true,[]]') {
  throw new Error("expected delete to compact later native values");
}

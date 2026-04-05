// Repo-authored idiomatic lowering fixture for fresh object spread from a
// native source object.

const src = { keep: 1, nested: [] };
const clone = { ...src };

clone.keep = 7;

if (JSON.stringify(src) !== '{"keep":1,"nested":[]}') {
  throw new Error("expected native object spread source to remain unchanged");
}

if (JSON.stringify(clone) !== '{"keep":7,"nested":[]}') {
  throw new Error("expected native object spread clone to support independent overwrite");
}

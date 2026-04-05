// Repo-authored idiomatic lowering fixture for fresh shallow own-property
// cloning from one native object into a new native object.

const src = { keep: true, items: [] };
const clone = Object.assign({}, src);

clone.keep = false;

if (JSON.stringify(src) !== '{"keep":true,"items":[]}') {
  throw new Error("expected native source to remain unchanged after fresh clone mutation");
}

if (JSON.stringify(clone) !== '{"keep":false,"items":[]}') {
  throw new Error("expected fresh native clone to preserve key order and allow independent top-level writes");
}

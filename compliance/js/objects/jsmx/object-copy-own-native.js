// Repo-authored idiomatic lowering fixture for shallow own-property copy from
// one native object into another through explicit jsval helper calls.

const dst = { keep: true, drop: 7 };
const src = { drop: 8, items: [] };

Object.assign(dst, src);

if (JSON.stringify(dst) !== '{"keep":true,"drop":8,"items":[]}') {
  throw new Error("expected native own-property copy to overwrite in place and append new keys");
}

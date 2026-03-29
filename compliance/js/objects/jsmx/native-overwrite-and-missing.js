// Repo-authored idiomatic lowering fixture for native overwrite semantics and
// missing reads.

const root = { keep: 7, items: [1, 2], nested: { flag: true } };

root.keep = 9;
root.items[1] = 8;

if (root.keep !== 9) {
  throw new Error("expected native object overwrite to replace the stored value");
}

if (root.missing !== undefined) {
  throw new Error("expected missing native property reads to return undefined");
}

if (root.items[4] !== undefined) {
  throw new Error("expected out-of-range native array reads to return undefined");
}

if (JSON.stringify(root) !== '{"keep":9,"items":[1,8],"nested":{"flag":true}}') {
  throw new Error("expected native overwrite lowering to preserve object size and array length");
}

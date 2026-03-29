// Repo-authored idiomatic lowering fixture for JSON-backed lookup semantics.

const root = JSON.parse('{"keep":7,"items":[1,2],"nested":{"flag":true}}');

if (root.keep !== 7) {
  throw new Error("expected JSON-backed property reads to preserve numeric values");
}

if (root.missing !== undefined) {
  throw new Error("expected missing JSON-backed property reads to return undefined");
}

if (!Object.prototype.hasOwnProperty.call(root, "keep")) {
  throw new Error("expected keep to be an own property before promotion");
}

if (Object.prototype.hasOwnProperty.call(root, "missing")) {
  throw new Error("expected missing to stay absent before promotion");
}

if (root.items[4] !== undefined) {
  throw new Error("expected out-of-range JSON-backed array reads to return undefined");
}

if (JSON.stringify(root) !== '{"keep":7,"items":[1,2],"nested":{"flag":true}}') {
  throw new Error("expected lookup-only lowering to preserve the original JSON document");
}

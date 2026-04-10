// Repo-authored idiomatic lowering fixture for native function value semantics.

const named = function sum(a, b) {
  return a + b;
};
const alias = named;
const other = function sum(a, b) {
  return a + b;
};

if (typeof named !== "function") {
  throw new Error("expected typeof function value to stay function");
}

if (!named) {
  throw new Error("expected function value to stay truthy");
}

if (named !== alias || named != alias || named === other || named == other) {
  throw new Error("expected function identity to stay site-sensitive under strict and abstract equality");
}

if (named.name !== "sum" || named.length !== 2) {
  throw new Error("expected function metadata to stay observable");
}

if (named(20, 22) !== 42) {
  throw new Error("expected direct function call to preserve the helper result");
}

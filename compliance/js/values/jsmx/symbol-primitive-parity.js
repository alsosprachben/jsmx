// Repo-authored idiomatic lowering fixture for native Symbol value semantics.

const a = Symbol("token");
const b = Symbol("token");
const none = Symbol();

if (typeof a !== "symbol") {
  throw new Error("expected typeof Symbol() to stay symbol");
}

if (!a || a === b || a == b) {
  throw new Error("expected distinct Symbols to stay unique under strict and abstract equality");
}

if (a.description !== "token" || none.description !== undefined) {
  throw new Error("expected Symbol descriptions to stay observable without implicit string coercion");
}

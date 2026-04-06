// Repo-authored idiomatic lowering fixture for typeof on regex-backed objects.

const regex = /a/g;
const iterator = "a".matchAll(regex);

if (typeof regex !== "object") {
  throw new Error("expected typeof regexp to stay object");
}

if (typeof iterator !== "object") {
  throw new Error("expected typeof matchAll iterator to stay object");
}

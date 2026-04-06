// Repo-authored idiomatic lowering fixture for regex-backed nullish and ternary values.

const regex = /a/g;
const iterator = "a".matchAll(regex);
const fallback = {};
const other = {};

if ((regex ?? fallback) !== regex) {
  throw new Error("expected regexp ?? fallback to preserve the regexp object");
}

if ((iterator ?? fallback) !== iterator) {
  throw new Error("expected matchAll iterator ?? fallback to preserve the iterator object");
}

if ((regex ? regex : other) !== regex) {
  throw new Error("expected regexp truthiness to preserve the regexp object");
}

if ((iterator ? iterator : other) !== iterator) {
  throw new Error("expected iterator truthiness to preserve the iterator object");
}

// Repo-authored idiomatic lowering fixture for primitive nullish coalescing.

if ((undefined ?? "fallback") !== "fallback") {
  throw new Error("expected undefined ?? fallback to pick the fallback");
}

if ((null ?? "fallback") !== "fallback") {
  throw new Error("expected null ?? fallback to pick the fallback");
}

if ((false ?? "fallback") !== false) {
  throw new Error("expected false ?? fallback to preserve false");
}

if ((0 ?? "fallback") !== 0) {
  throw new Error("expected 0 ?? fallback to preserve 0");
}

if (("" ?? "fallback") !== "") {
  throw new Error("expected empty string ?? fallback to preserve the empty string");
}

if (("x" ?? "fallback") !== "x") {
  throw new Error("expected non-nullish string ?? fallback to preserve the string");
}

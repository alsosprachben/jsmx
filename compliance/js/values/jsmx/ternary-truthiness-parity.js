// Repo-authored idiomatic lowering fixture for ternary truthiness parity.

if ((undefined ? "then" : "else") !== "else") {
  throw new Error("expected undefined to take the ternary else branch");
}

if ((null ? "then" : "else") !== "else") {
  throw new Error("expected null to take the ternary else branch");
}

if ((false ? "then" : "else") !== "else") {
  throw new Error("expected false to take the ternary else branch");
}

if ((0 ? "then" : "else") !== "else") {
  throw new Error("expected zero to take the ternary else branch");
}

if (("" ? "then" : "else") !== "else") {
  throw new Error("expected empty string to take the ternary else branch");
}

if (("x" ? "then" : "else") !== "then") {
  throw new Error("expected nonempty string to take the ternary then branch");
}

if ((1 ? "then" : "else") !== "then") {
  throw new Error("expected nonzero number to take the ternary then branch");
}

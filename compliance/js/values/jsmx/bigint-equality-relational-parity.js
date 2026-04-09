// Repo-authored idiomatic lowering fixture for BigInt equality and relational comparisons.

const answer = 42n;

if (!(answer == 42) || answer == 42.5) {
  throw new Error("expected abstract equality to accept exact integral numbers and reject fractional numbers");
}

if (!(answer < 42.5)) {
  throw new Error("expected bigint < fractional number to preserve mathematical ordering");
}

if (!(100n > "42")) {
  throw new Error("expected bigint > parseable decimal string to preserve mathematical ordering");
}

if (!(answer <= "42")) {
  throw new Error("expected bigint <= identical decimal string to stay true");
}

if (answer < "42.5" || answer > "42.5") {
  throw new Error("expected non-BigInt decimal strings to stay unordered against BigInt");
}

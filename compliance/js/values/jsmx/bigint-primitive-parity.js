// Repo-authored idiomatic lowering fixture for native BigInt primitive semantics.

const zero = 0n;
const answer = 42n;
const duplicate = 42n;

if (typeof zero !== "bigint") {
  throw new Error("expected typeof 0n to stay bigint");
}

if (zero || !answer) {
  throw new Error("expected 0n to stay falsy and nonzero BigInt values to stay truthy");
}

if (answer !== duplicate || answer != "42") {
  throw new Error("expected BigInt equality to stay value-based across strict and string abstract equality");
}

if (answer == "42.5" || answer == null) {
  throw new Error("expected non-integral strings and null to stay abstract-inequal to BigInt");
}

// Repo-authored idiomatic lowering fixture for the explicit capacity contract
// on fresh dense-array cloning.

const src = JSON.parse("[1,2]");

try {
  void src;
} catch (err) {
  throw new Error("unexpected error while preparing array clone capacity fixture");
}

// The generated C fixture asserts the ENOBUFS contract directly because the
// flattened helper keeps dense-array capacity planning explicit in C.

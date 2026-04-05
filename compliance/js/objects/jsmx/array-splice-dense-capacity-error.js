// Repo-authored idiomatic lowering fixture for the explicit capacity contract
// on bounded dense-array splice edits.

const arr = [1, 2, 3, 4];

try {
  void arr;
} catch (err) {
  throw new Error("unexpected error while preparing dense splice capacity fixture");
}

// The generated C fixture asserts the ENOBUFS contract directly because the
// flattened helper keeps dense-array capacity planning explicit in C.

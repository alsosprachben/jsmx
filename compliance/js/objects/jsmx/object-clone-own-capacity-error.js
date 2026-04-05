// Repo-authored idiomatic lowering fixture for the explicit capacity contract
// on fresh shallow own-property cloning.

const src = JSON.parse('{"a":1,"b":2}');

try {
  void src;
} catch (err) {
  throw new Error("unexpected error while preparing clone capacity fixture");
}

// The generated C fixture asserts the ENOBUFS contract directly because the
// flattened helper keeps capacity planning explicit in C rather than in JS.

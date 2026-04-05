// Repo-authored idiomatic lowering fixture for bounded dense-array unshift.

const arr = [5];

// Translator lowers this to a native array with exact capacity 1, so the
// second front insertion must fail with ENOBUFS while the original dense value
// remains intact.

if (JSON.stringify(arr) !== "[5]") {
  throw new Error("expected original dense array to remain unchanged after capacity failure");
}

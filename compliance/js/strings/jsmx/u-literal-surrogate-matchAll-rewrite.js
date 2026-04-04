// Repo-authored rewrite-backed fixture for String.prototype.matchAll over a
// single literal lone-surrogate `/u` atom.
//
// The translator-owned helper must yield exec-shaped iterator results for
// isolated surrogate code units while skipping surrogate halves inside pairs.

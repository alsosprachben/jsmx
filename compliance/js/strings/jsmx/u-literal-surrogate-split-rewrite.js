// Repo-authored rewrite-backed fixture for String.prototype.split over a
// single literal lone-surrogate `/u` atom.
//
// The surrogate-aware split helper must skip surrogate halves inside a pair,
// but still split on isolated high or low surrogate code units.

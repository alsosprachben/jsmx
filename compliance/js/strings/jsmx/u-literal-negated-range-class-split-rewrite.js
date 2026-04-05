// Repo-authored rewrite-backed fixture for String.prototype.split over a
// single negated no-capture `/u` character class with explicit inclusive
// UTF-16 ranges.
//
// The approved family uses the dedicated negated-range-class split helper so
// separators match only isolated boundary-aligned code units outside the
// explicit exclusion ranges.

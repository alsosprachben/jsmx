// Repo-authored rewrite-backed fixture for String.prototype.split over a
// single no-capture `/u` character class with explicit inclusive UTF-16
// ranges.
//
// The approved family uses the dedicated range-class split helper so
// separators only match isolated boundary-aligned code units.

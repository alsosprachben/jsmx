// Repo-authored rewrite-backed fixture for String.prototype.split over a
// single negated no-capture `/u` character class with explicit literal
// members.
//
// The helper must only split on isolated boundary-aligned matches and must
// never consume a surrogate-pair half.

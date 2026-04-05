// Repo-authored rewrite-backed fixture for String.prototype.search over a
// single negated no-capture `/u` character class with explicit inclusive
// UTF-16 ranges.
//
// The approved family passes explicit range-pair exclusions directly to the
// dedicated negated-range-class helpers rather than lowering through
// jsval_regexp_new(...).

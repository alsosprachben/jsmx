// Repo-authored rewrite-backed fixture for String.prototype.search over a
// single no-capture `/u` character class with explicit inclusive UTF-16
// ranges.
//
// The approved family passes explicit `[start, end]` range pairs directly to
// the dedicated range-class helpers rather than lowering through
// jsval_regexp_new(...).

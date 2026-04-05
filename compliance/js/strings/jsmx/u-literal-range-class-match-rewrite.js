// Repo-authored rewrite-backed fixture for String.prototype.match and
// String.prototype.matchAll over a single no-capture `/u` character class with
// explicit inclusive UTF-16 ranges.
//
// The approved family passes explicit range-pair tables directly to the
// dedicated jsval range-class helpers so boundary-aware one-unit results stay
// out of the ordinary regex lowering path.

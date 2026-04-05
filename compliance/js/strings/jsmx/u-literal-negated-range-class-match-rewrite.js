// Repo-authored rewrite-backed fixture for String.prototype.match and
// String.prototype.matchAll over a single negated no-capture `/u` character
// class with explicit inclusive UTF-16 ranges.
//
// The approved family keeps the translator-owned exclusion table explicit so
// boundary-aware one-unit matches avoid the ordinary regex lowering path.

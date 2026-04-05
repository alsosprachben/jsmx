// Repo-authored rewrite-backed fixture for String.prototype.match and
// String.prototype.matchAll over a single negated no-capture `/u` character
// class with explicit literal members.
//
// This family remains translator-owned and boundary-aware: intact surrogate
// pairs stay indivisible code points during scanning.

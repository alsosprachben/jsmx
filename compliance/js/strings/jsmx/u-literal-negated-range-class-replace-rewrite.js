// Repo-authored rewrite-backed fixture for String.prototype.replace and
// String.prototype.replaceAll over a single negated no-capture `/u` character
// class with explicit inclusive UTF-16 ranges.
//
// The approved family lowers directly to negated-range-class helper calls,
// including callback replacers, so replacements stay boundary-aware without
// broadening ordinary regex semantics.

// Repo-authored rewrite-backed fixture for String.prototype.replace and
// String.prototype.replaceAll over a single no-capture `/u` character class
// with explicit inclusive UTF-16 ranges.
//
// The approved family lowers directly to range-class helper calls, including
// callback replacers, so substitutions stay boundary-aware without expanding
// the ordinary regex surface.

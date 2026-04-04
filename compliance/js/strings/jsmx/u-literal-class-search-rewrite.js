// Repo-authored rewrite-backed fixture for String.prototype.search over a
// single no-capture `/u` character class with explicit literal members.
//
// The approved family passes literal class members directly to the dedicated
// class helpers rather than lowering through jsval_regexp_new(...).

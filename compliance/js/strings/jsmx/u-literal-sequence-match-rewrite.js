// Repo-authored rewrite-backed fixture for String.prototype.match and
// String.prototype.matchAll over fixed literal `/u` UTF-16 sequences.
//
// The approved family is still no-capture only; generated C uses the
// dedicated sequence helpers rather than lowering through jsval_regexp_new(...).

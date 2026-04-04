// Repo-authored rewrite-backed fixture for String.prototype.match over a
// single literal lone-surrogate `/u` atom.
//
// This family is translator-owned: generated C uses the narrow
// jsval_method_string_match_u_literal_surrogate(...) helper instead of
// lowering through jsval_regexp_new(...).

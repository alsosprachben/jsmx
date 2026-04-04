// Repo-authored rewrite-backed fixture for lone-surrogate `/u` exec/test.
//
// This covers the reviewed translator-owned rewrite family where a single
// literal surrogate atom under the Unicode flag is lowered to explicit UTF-16
// matching logic rather than the PCRE2-backed regexp constructor path.

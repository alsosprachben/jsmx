// Repo-authored rewrite-backed fixture for lone-surrogate `/u` search.
//
// This covers the translator-owned rewrite family where a single literal
// surrogate atom under the Unicode flag is lowered to explicit UTF-16 search
// logic instead of the PCRE2-backed regex-search path.

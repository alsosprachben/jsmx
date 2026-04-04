// Repo-authored rewrite-backed fixture for String.prototype.replace and
// replaceAll over a single literal lone-surrogate `/u` atom.
//
// The reviewed rewrite keeps no-capture semantics and uses the dedicated
// surrogate-aware jsval replace helpers instead of the PCRE2-backed regex
// method path.

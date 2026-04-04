// Repo-authored rewrite-backed fixture for String.prototype.search over a
// fixed literal `/u` UTF-16 sequence with no captures.
//
// The translator-owned helper must only match at Unicode code-point
// boundaries, so sequences that would start or end inside a surrogate pair
// are rejected even when the raw code units align.

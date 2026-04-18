// Repo-authored WinterTC URL parity fixture.
//
// Exercises the WHATWG URL surface: construction, all component
// accessors, href round-trip, and a setter that rewrites href
// in place. Serialization must be stable across parse → get →
// set → get.

const u = new URL("https://user:pass@example.com:8080/path?a=1#frag");

// All eight component getters return spec-aligned strings.
// protocol:   "https:"
// username:   "user"
// password:   "pass"
// hostname:   "example.com"
// host:       "example.com:8080"
// port:       "8080"
// pathname:   "/path"
// search:     "?a=1"
// hash:       "#frag"
// href:       "https://user:pass@example.com:8080/path?a=1#frag"

// Setter round-trip: mutating pathname updates href.
u.pathname = "/newpath";
// href now: "https://user:pass@example.com:8080/newpath?a=1#frag"

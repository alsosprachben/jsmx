// Repo-authored WinterTC URL static-methods parity fixture.
//
// URL.canParse(input, base?) returns a boolean and never
// throws. URL.parse(input, base?) returns the URL or null and
// never throws. Both are non-throwing counterparts to
// `new URL(...)`.

// Happy path.
const ok_bool = URL.canParse("https://example.com/");   // true
const ok_url  = URL.parse("https://example.com/");      // URL instance

// Relative URL without a base — spec says false/null, not throw.
const bad_bool = URL.canParse("/just/a/path");           // false
const bad_url  = URL.parse("/just/a/path");              // null

// Relative URL with a base resolves.
const rel_bool = URL.canParse("/foo", "https://example.com/");  // true
const rel_url  = URL.parse("/foo", "https://example.com/");     // URL
// rel_url.href === "https://example.com/foo"

// Invalid hostname — false/null (not throw).
const inv_bool = URL.canParse("https://a..b/");         // false
const inv_url  = URL.parse("https://a..b/");            // null

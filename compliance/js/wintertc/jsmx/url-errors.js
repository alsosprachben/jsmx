// Repo-authored WinterTC URL error-path fixture.
//
// Exercises jsval_url_new's negative paths: invalid URL syntax
// and relative URLs without a base. jsmx surfaces these as a
// return code + errno, parallel to how other WinterTC fixtures
// (fetch-response-errors, crypto-subtle-digest-errors) verify
// negative outcomes.

// Relative URL without a base → TypeError in the spec, errno
// in jsmx.
let threw1 = false;
try { new URL("/just/a/path"); } catch (e) { threw1 = true; }
// threw1 === true

// Invalid hostname (empty label) → TypeError in the spec, errno
// in jsmx.
let threw2 = false;
try { new URL("https://a..b/"); } catch (e) { threw2 = true; }
// threw2 === true

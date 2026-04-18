// Repo-authored JSON escape-codec fixture.
//
// JSON.parse / JSON.stringify must round-trip every spec-
// required string escape without losing or re-escaping bytes.

// Simple escapes: \", \\, \n, \t
const simple = JSON.parse('"line\\nfeed\\"quote\\\\back\\ttab"');
// simple === 'line\nfeed"quote\\back\ttab'
// JSON.stringify(simple) returns the same escape sequence

// BMP \uXXXX
const bmp = JSON.parse('"\\u0041"');
// bmp === "A"
// JSON.stringify(bmp) === '"A"' (codec strips unnecessary escape)

// Surrogate pair: U+1F600 (😀) as \ud83d\ude00
const emoji = JSON.parse('"\\ud83d\\ude00"');
// emoji is a 1-codepoint string containing U+1F600

// Plain string with no escapes round-trips unmodified.
const plain = JSON.parse('"plain"');
// plain === "plain"; stringified === '"plain"'

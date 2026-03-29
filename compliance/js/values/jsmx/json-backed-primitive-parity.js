// Repo-authored idiomatic lowering fixture for JSON-backed primitive parity.

const parsed = JSON.parse('{"num":1,"flag":true,"off":false,"text":"2","nothing":null}');

if (parsed.num !== 1) {
  throw new Error("expected parsed number to preserve strict equality with the native literal");
}

if (parsed.flag !== true || parsed.off !== false) {
  throw new Error("expected parsed booleans to preserve strict equality with native booleans");
}

if (parsed.text !== "2") {
  throw new Error("expected parsed string to preserve strict equality with the native literal");
}

if (parsed.nothing !== null) {
  throw new Error("expected parsed null to preserve strict equality with native null");
}

if ((parsed.text + 1) !== "21") {
  throw new Error("expected parsed string addition to preserve concatenation behavior");
}

if ((parsed.flag + 1) !== 2) {
  throw new Error("expected parsed true + 1 to preserve numeric addition");
}

if ((parsed.nothing + 1) !== 1) {
  throw new Error("expected parsed null + 1 to preserve numeric addition");
}

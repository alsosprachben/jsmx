// Repo-authored idiomatic lowering fixture for parsed/native nullish coalescing parity.

const parsed = JSON.parse('{"nothing":null,"flag":false,"zero":0,"empty":"","text":"x","obj":{}}');
const nativeObj = {};

if ((parsed.nothing ?? "fallback") !== "fallback") {
  throw new Error("expected parsed null ?? fallback to pick the fallback");
}

if ((parsed.flag ?? "fallback") !== false) {
  throw new Error("expected parsed false ?? fallback to preserve false");
}

if ((parsed.zero ?? "fallback") !== 0) {
  throw new Error("expected parsed zero ?? fallback to preserve zero");
}

if ((parsed.empty ?? "fallback") !== "") {
  throw new Error("expected parsed empty string ?? fallback to preserve the string");
}

if ((parsed.text ?? "fallback") !== "x") {
  throw new Error("expected parsed text ?? fallback to preserve the string");
}

if ((parsed.obj ?? nativeObj) !== parsed.obj) {
  throw new Error("expected parsed object ?? fallback to preserve object identity");
}

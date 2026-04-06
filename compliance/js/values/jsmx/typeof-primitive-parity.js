// Repo-authored idiomatic lowering fixture for typeof on flattened primitives.

const parsed = JSON.parse('{"flag":true,"num":1,"text":"x","nothing":null}');

if (typeof undefined !== "undefined") {
  throw new Error("expected typeof undefined to stay undefined");
}

if (typeof null !== "object" || typeof parsed.nothing !== "object") {
  throw new Error("expected typeof null to stay object across native and parsed values");
}

if (typeof true !== "boolean" || typeof parsed.flag !== "boolean") {
  throw new Error("expected typeof boolean to stay boolean across native and parsed values");
}

if (typeof 1 !== "number" || typeof parsed.num !== "number") {
  throw new Error("expected typeof number to stay number across native and parsed values");
}

if (typeof "x" !== "string" || typeof parsed.text !== "string") {
  throw new Error("expected typeof string to stay string across native and parsed values");
}

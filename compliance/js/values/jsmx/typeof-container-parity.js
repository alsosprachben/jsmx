// Repo-authored idiomatic lowering fixture for typeof on container values.

const parsed = JSON.parse('{"obj":{},"arr":[]}');
const nativeObj = {};
const nativeArr = [];

if (typeof parsed !== "object") {
  throw new Error("expected typeof parsed root object to stay object");
}

if (typeof nativeObj !== "object" || typeof parsed.obj !== "object") {
  throw new Error("expected typeof object values to stay object across native and parsed values");
}

if (typeof nativeArr !== "object" || typeof parsed.arr !== "object") {
  throw new Error("expected typeof array values to stay object across native and parsed values");
}

// Repo-authored idiomatic lowering fixture for bounded ISO Date parsing and wire stringify helpers.

const text = "2020-01-02T03:04:05.006Z";
const time = Date.parse(text);
const date = new Date(text);

if (time !== 1577934245006 || date.getTime() !== 1577934245006) {
  throw new Error("expected ISO parse to preserve the exact epoch milliseconds");
}

if (date.toISOString() !== text || date.toJSON() !== text) {
  throw new Error("expected ISO and JSON stringification to preserve the canonical wire form");
}

if (date.toUTCString() !== "Thu, 02 Jan 2020 03:04:05 GMT") {
  throw new Error("expected UTC stringification to preserve the canonical UTC fields");
}

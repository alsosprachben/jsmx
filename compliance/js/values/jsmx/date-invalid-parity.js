// Repo-authored idiomatic lowering fixture for invalid Date behavior.

const date = new Date(NaN);

if (!Number.isNaN(date.getTime()) || !Number.isNaN(date.valueOf())) {
  throw new Error("expected invalid Date time-value reads to stay NaN");
}

if (date.toString() !== "Invalid Date" || date.toUTCString() !== "Invalid Date") {
  throw new Error("expected invalid Date string helpers to stay Invalid Date");
}

if (date.toJSON() !== null) {
  throw new Error("expected invalid Date JSON helper to stay null");
}

let sawRange = false;
try {
  date.toISOString();
} catch (error) {
  sawRange = error instanceof RangeError;
}

if (!sawRange) {
  throw new Error("expected invalid Date toISOString to throw RangeError");
}

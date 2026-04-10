// Repo-authored idiomatic lowering fixture for native Date identity/value semantics.

const date = new Date(1577934245006);
const alias = date;
const other = new Date(1577934245006);

if (typeof date !== "object") {
  throw new Error("expected typeof Date value to stay object");
}

if (!date) {
  throw new Error("expected Date value to stay truthy");
}

if (date !== alias || date === other) {
  throw new Error("expected Date identity to stay alias-sensitive");
}

if (date.getTime() !== 1577934245006 || date.valueOf() !== 1577934245006) {
  throw new Error("expected Date time-value helpers to expose the stored epoch milliseconds");
}

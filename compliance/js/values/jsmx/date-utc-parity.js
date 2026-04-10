// Repo-authored idiomatic lowering fixture for UTC Date construction and field updates.

const date = new Date(Date.UTC(99, 0, 2, 3, 4, 5, 6));

if (date.getUTCFullYear() !== 1999 || date.getUTCMonth() !== 0 || date.getUTCDate() !== 2) {
  throw new Error("expected Date.UTC construction to preserve the JS 0-99 year offset");
}

if (date.getUTCDay() !== 6 || date.getUTCHours() !== 3 || date.getUTCMinutes() !== 4) {
  throw new Error("expected UTC getters to expose the stored components");
}

date.setUTCMonth(1);
if (date.getUTCMonth() !== 1 || date.getUTCDate() !== 2) {
  throw new Error("expected setUTCMonth to preserve the day when it still fits");
}

date.setUTCDate(29);
if (date.getUTCMonth() !== 2 || date.getUTCDate() !== 1) {
  throw new Error("expected setUTCDate to normalize across month boundaries");
}

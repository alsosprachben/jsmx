// Repo-authored idiomatic lowering fixture for local-field Date construction and mutation.

const date = new Date(99, 0, 2, 3, 4, 5, 6);

if (date.getFullYear() !== 1999 || date.getMonth() !== 0 || date.getDate() !== 2) {
  throw new Error("expected local-field Date construction to preserve the local components");
}

if (date.getHours() !== 3 || date.getMinutes() !== 4 || date.getSeconds() !== 5 || date.getMilliseconds() !== 6) {
  throw new Error("expected local getters to expose the stored local components");
}

date.setMonth(1);
date.setHours(23);

if (date.getMonth() !== 1 || date.getHours() !== 23) {
  throw new Error("expected local setters to update the live local-time state");
}

// Repo-authored WinterTC getRandomValues parity fixture.
const bytes = new Uint8Array(16);
const before = bytes.slice();
const result = crypto.getRandomValues(bytes);

[
  result === bytes,
  bytes.length,
  before.join(",") !== bytes.join(","),
];

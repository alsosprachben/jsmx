// Repo-authored WinterTC subtle.digest rejection fixture.
const subtle = globalThis.crypto.subtle;
const input = new Uint8Array(16);

[
  subtle.digest("SHA-999", input),
  subtle.digest("SHA-256", 5),
];

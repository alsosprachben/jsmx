// Repo-authored WinterTC HMAC parity fixture.
const subtle = globalThis.crypto.subtle;
const generated = subtle.generateKey(
  { name: "HMAC", hash: { name: "SHA-256" } },
  true,
  ["sign", "verify"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(32),
  { name: "HMAC", hash: { name: "SHA-256" } },
  true,
  ["sign"],
);
const jwkImported = subtle.importKey(
  "jwk",
  { kty: "oct", k: "__8", alg: "HS256", ext: true, key_ops: ["sign"] },
  { name: "HMAC", hash: { name: "SHA-256" }, length: 9 },
  true,
  ["sign"],
);

[generated, rawImported, jwkImported];

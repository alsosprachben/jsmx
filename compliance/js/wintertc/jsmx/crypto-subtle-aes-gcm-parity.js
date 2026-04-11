// Repo-authored WinterTC AES-GCM parity fixture.
const subtle = globalThis.crypto.subtle;
const params = { name: "AES-GCM", iv: new Uint8Array(12) };

const generated = subtle.generateKey(
  { name: "AES-GCM", length: 256 },
  true,
  ["encrypt", "decrypt"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-GCM" },
  true,
  ["encrypt", "decrypt"],
);
const jwkImported = subtle.importKey(
  "jwk",
  {
    kty: "oct",
    k: "AAAAAAAAAAAAAAAAAAAAAA",
    alg: "A128GCM",
    ext: true,
    key_ops: ["encrypt", "decrypt"],
  },
  { name: "AES-GCM" },
  true,
  ["encrypt", "decrypt"],
);
const encrypted = subtle.encrypt(params, rawImported, new Uint8Array(16));

[generated, rawImported, jwkImported, encrypted];

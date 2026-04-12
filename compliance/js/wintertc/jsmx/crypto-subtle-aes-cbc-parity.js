// Repo-authored WinterTC AES-CBC parity fixture.
const subtle = globalThis.crypto.subtle;
const params = { name: "AES-CBC", iv: new Uint8Array(16) };

const generated = subtle.generateKey(
  { name: "AES-CBC", length: 256 },
  true,
  ["encrypt", "decrypt"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-CBC" },
  true,
  ["encrypt", "decrypt"],
);
const jwkImported = subtle.importKey(
  "jwk",
  {
    kty: "oct",
    k: "AAAAAAAAAAAAAAAAAAAAAA",
    alg: "A128CBC",
    ext: true,
    key_ops: ["encrypt", "decrypt"],
  },
  { name: "AES-CBC" },
  true,
  ["encrypt", "decrypt"],
);
const encrypted = subtle.encrypt(params, rawImported, new Uint8Array(16));

[generated, rawImported, jwkImported, encrypted];

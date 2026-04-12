// Repo-authored WinterTC AES-CTR parity fixture.
const subtle = globalThis.crypto.subtle;
const params = {
  name: "AES-CTR",
  counter: new Uint8Array(16),
  length: 128,
};

const generated = subtle.generateKey(
  { name: "AES-CTR", length: 256 },
  true,
  ["encrypt", "decrypt"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-CTR" },
  true,
  ["encrypt", "decrypt"],
);
const jwkImported = subtle.importKey(
  "jwk",
  {
    kty: "oct",
    k: "AAAAAAAAAAAAAAAAAAAAAA",
    alg: "A128CTR",
    ext: true,
    key_ops: ["encrypt", "decrypt"],
  },
  { name: "AES-CTR" },
  true,
  ["encrypt", "decrypt"],
);
const encrypted = subtle.encrypt(params, rawImported, new Uint8Array(16));

[generated, rawImported, jwkImported, encrypted];

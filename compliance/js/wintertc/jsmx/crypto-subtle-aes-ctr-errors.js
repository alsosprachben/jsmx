// Repo-authored WinterTC AES-CTR rejection fixture.
const subtle = globalThis.crypto.subtle;

const nonExtractable = subtle.generateKey(
  { name: "AES-CTR", length: 128 },
  false,
  ["encrypt", "decrypt"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-CTR" },
  true,
  ["encrypt", "decrypt"],
);

[
  nonExtractable,
  subtle.importKey(
    "pkcs8",
    new Uint8Array(16),
    { name: "AES-CTR" },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.importKey(
    "jwk",
    {
      kty: "RSA",
      k: "AAAAAAAAAAAAAAAAAAAAAA",
      alg: "A128CTR",
      ext: true,
      key_ops: ["encrypt", "decrypt"],
    },
    { name: "AES-CTR" },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.encrypt(
    { name: "AES-CTR", counter: new Uint8Array(12), length: 64 },
    rawImported,
    new Uint8Array(16),
  ),
  subtle.encrypt(
    { name: "AES-CTR", counter: new Uint8Array(16), length: 0 },
    rawImported,
    new Uint8Array(16),
  ),
];

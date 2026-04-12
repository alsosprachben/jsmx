// Repo-authored WinterTC AES-CBC rejection fixture.
const subtle = globalThis.crypto.subtle;

const nonExtractable = subtle.generateKey(
  { name: "AES-CBC", length: 128 },
  false,
  ["encrypt", "decrypt"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-CBC" },
  true,
  ["encrypt", "decrypt"],
);

[
  nonExtractable,
  subtle.importKey(
    "pkcs8",
    new Uint8Array(16),
    { name: "AES-CBC" },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.importKey(
    "jwk",
    {
      kty: "RSA",
      k: "AAAAAAAAAAAAAAAAAAAAAA",
      alg: "A128CBC",
      ext: true,
      key_ops: ["encrypt", "decrypt"],
    },
    { name: "AES-CBC" },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.encrypt(
    { name: "AES-CBC", iv: new Uint8Array(12) },
    rawImported,
    new Uint8Array(16),
  ),
];

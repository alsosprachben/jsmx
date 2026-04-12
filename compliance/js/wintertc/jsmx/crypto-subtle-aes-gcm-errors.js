// Repo-authored WinterTC AES-GCM rejection fixture.
const subtle = globalThis.crypto.subtle;

const nonExtractable = subtle.generateKey(
  { name: "AES-GCM", length: 128 },
  false,
  ["encrypt", "decrypt"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-GCM" },
  true,
  ["encrypt", "decrypt"],
);

[
  nonExtractable,
  subtle.importKey(
    "pkcs8",
    new Uint8Array(16),
    { name: "AES-GCM" },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.importKey(
    "jwk",
    {
      kty: "RSA",
      k: "AAAAAAAAAAAAAAAAAAAAAA",
      alg: "A128GCM",
      ext: true,
      key_ops: ["encrypt", "decrypt"],
    },
    { name: "AES-GCM" },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.encrypt({ name: "AES-GCM" }, rawImported, new Uint8Array(16)),
  subtle.encrypt(
    { name: "AES-GCM", iv: new Uint8Array(12), tagLength: 40 },
    rawImported,
    new Uint8Array(16),
  ),
  subtle.wrapKey(
    "raw",
    rawImported,
    rawImported,
    { name: "AES-GCM" },
  ),
];

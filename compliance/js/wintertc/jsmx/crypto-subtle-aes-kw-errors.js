// Repo-authored WinterTC AES-KW + wrapKey/unwrapKey rejection fixture.
const subtle = globalThis.crypto.subtle;

const nonExtractable = subtle.generateKey(
  { name: "AES-KW", length: 128 },
  false,
  ["wrapKey", "unwrapKey"],
);
const wrappingKey = subtle.generateKey(
  { name: "AES-KW", length: 128 },
  true,
  ["wrapKey", "unwrapKey"],
);

[
  nonExtractable,
  subtle.importKey(
    "pkcs8",
    new Uint8Array(16),
    { name: "AES-KW" },
    true,
    ["wrapKey", "unwrapKey"],
  ),
  subtle.importKey(
    "jwk",
    {
      kty: "RSA",
      k: "AAAAAAAAAAAAAAAAAAAAAA",
      alg: "A128KW",
      ext: true,
      key_ops: ["wrapKey", "unwrapKey"],
    },
    { name: "AES-KW" },
    true,
    ["wrapKey", "unwrapKey"],
  ),
  subtle.unwrapKey(
    "raw",
    new Uint8Array(7),
    wrappingKey,
    { name: "AES-KW" },
    { name: "AES-GCM", length: 128 },
    true,
    ["encrypt", "decrypt"],
  ),
  subtle.unwrapKey(
    "raw",
    new Uint8Array(24),
    wrappingKey,
    { name: "AES-KW" },
    { name: "RSA-OAEP" },
    true,
    ["encrypt", "decrypt"],
  ),
];

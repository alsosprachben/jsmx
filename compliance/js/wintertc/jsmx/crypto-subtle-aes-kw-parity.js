// Repo-authored WinterTC AES-KW + wrapKey/unwrapKey parity fixture.
const subtle = globalThis.crypto.subtle;

const wrappingKey = subtle.generateKey(
  { name: "AES-KW", length: 128 },
  true,
  ["wrapKey", "unwrapKey"],
);
const rawImported = subtle.importKey(
  "raw",
  new Uint8Array(16),
  { name: "AES-KW" },
  true,
  ["wrapKey", "unwrapKey"],
);
const jwkImported = subtle.importKey(
  "jwk",
  {
    kty: "oct",
    k: "AAAAAAAAAAAAAAAAAAAAAA",
    alg: "A128KW",
    ext: true,
    key_ops: ["wrapKey", "unwrapKey"],
  },
  { name: "AES-KW" },
  true,
  ["wrapKey", "unwrapKey"],
);
const innerKey = subtle.generateKey(
  { name: "AES-GCM", length: 128 },
  true,
  ["encrypt", "decrypt"],
);
const wrapped = subtle.wrapKey("raw", innerKey, wrappingKey, { name: "AES-KW" });
const unwrapped = subtle.unwrapKey(
  "raw",
  new Uint8Array(24),
  wrappingKey,
  { name: "AES-KW" },
  { name: "AES-GCM", length: 128 },
  true,
  ["encrypt", "decrypt"],
);
const jwkWrapped = subtle.wrapKey(
  "jwk",
  innerKey,
  wrappingKey,
  { name: "AES-KW" },
);

[wrappingKey, rawImported, jwkImported, wrapped, unwrapped, jwkWrapped];

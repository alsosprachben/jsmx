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

const wrappingKey = subtle.generateKey(
  { name: "AES-GCM", length: 128 },
  true,
  ["wrapKey", "unwrapKey"],
);
const innerHmac = subtle.generateKey(
  { name: "HMAC", hash: { name: "SHA-256" } },
  true,
  ["sign", "verify"],
);
const wrapped = subtle.wrapKey("raw", innerHmac, wrappingKey, params);
const unwrapped = subtle.unwrapKey(
  "raw",
  wrapped,
  wrappingKey,
  params,
  { name: "HMAC", hash: { name: "SHA-256" } },
  true,
  ["sign", "verify"],
);

[generated, rawImported, jwkImported, encrypted, wrapped, unwrapped];

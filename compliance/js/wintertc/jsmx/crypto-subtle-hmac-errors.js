// Repo-authored WinterTC HMAC rejection fixture.
const subtle = globalThis.crypto.subtle;

[
  subtle.generateKey({ name: "HMAC", hash: { name: "SHA-256" } }, true, []),
  subtle.importKey(
    "pkcs8",
    new Uint8Array(32),
    { name: "HMAC", hash: { name: "SHA-256" } },
    true,
    ["sign"],
  ),
  subtle.importKey(
    "jwk",
    { kty: "RSA", k: "__8", alg: "HS256", ext: true, key_ops: ["sign"] },
    { name: "HMAC", hash: { name: "SHA-256" }, length: 9 },
    true,
    ["sign"],
  ),
];

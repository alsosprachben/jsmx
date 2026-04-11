// Repo-authored WinterTC PBKDF2 parity fixture.
const subtle = globalThis.crypto.subtle;
const params = {
  name: "PBKDF2",
  hash: { name: "SHA-256" },
  salt: new Uint8Array(8),
  iterations: 2,
};

const baseKey = subtle.importKey(
  "raw",
  new Uint8Array(8),
  { name: "PBKDF2" },
  true,
  ["deriveBits", "deriveKey"],
);

[
  baseKey,
  subtle.deriveBits(params, baseKey, 256),
  subtle.deriveKey(
    params,
    baseKey,
    { name: "HMAC", hash: { name: "SHA-256" }, length: 512 },
    true,
    ["sign", "verify"],
  ),
  subtle.deriveKey(
    params,
    baseKey,
    { name: "AES-GCM", length: 128 },
    true,
    ["encrypt", "decrypt"],
  ),
];

// Repo-authored WinterTC HKDF parity fixture.
const subtle = globalThis.crypto.subtle;
const params = {
  name: "HKDF",
  hash: { name: "SHA-256" },
  salt: new Uint8Array(8),
  info: new Uint8Array(8),
};

const baseKey = subtle.importKey(
  "raw",
  new Uint8Array(8),
  { name: "HKDF" },
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

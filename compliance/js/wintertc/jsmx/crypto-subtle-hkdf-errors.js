// Repo-authored WinterTC HKDF rejection fixture.
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
const deriveKeyOnly = subtle.importKey(
  "raw",
  new Uint8Array(8),
  { name: "HKDF" },
  true,
  ["deriveKey"],
);

[
  subtle.importKey("jwk", {}, { name: "HKDF" }, true, ["deriveBits"]),
  subtle.deriveBits(params, deriveKeyOnly, 256),
  subtle.deriveBits(
    { name: "HKDF", hash: { name: "SHA-256" }, info: new Uint8Array(8) },
    baseKey,
    256,
  ),
  subtle.deriveBits(
    { name: "HKDF", hash: { name: "SHA-256" }, salt: new Uint8Array(8) },
    baseKey,
    256,
  ),
  subtle.deriveBits(params, baseKey, 255),
  subtle.deriveKey(
    params,
    baseKey,
    { name: "AES-CBC", length: 128 },
    true,
    ["encrypt"],
  ),
];

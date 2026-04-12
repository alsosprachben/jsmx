// Repo-authored WinterTC PBKDF2 rejection fixture.
const subtle = globalThis.crypto.subtle;
const params = {
  name: "PBKDF2",
  hash: { name: "SHA-256" },
  salt: new Uint8Array(8),
  iterations: 2,
};
const deriveKeyOnly = subtle.importKey(
  "raw",
  new Uint8Array(8),
  { name: "PBKDF2" },
  true,
  ["deriveKey"],
);

[
  subtle.importKey("jwk", {}, { name: "PBKDF2" }, true, ["deriveBits"]),
  subtle.deriveBits(params, deriveKeyOnly, 256),
  subtle.deriveBits(
    { name: "PBKDF2", hash: { name: "SHA-256" }, iterations: 2 },
    deriveKeyOnly,
    256,
  ),
  subtle.deriveBits(
    {
      name: "PBKDF2",
      hash: { name: "SHA-256" },
      salt: new Uint8Array(8),
      iterations: 0,
    },
    deriveKeyOnly,
    256,
  ),
  subtle.deriveKey(
    params,
    deriveKeyOnly,
    { name: "AES-CBC", length: 128 },
    true,
    ["encrypt"],
  ),
];

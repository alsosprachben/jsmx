// Repo-authored WinterTC Ed25519 rejection fixture.
const subtle = globalThis.crypto.subtle;

const pair = subtle.generateKey(
  { name: "Ed25519" },
  true,
  ["sign", "verify"],
);
const data = new Uint8Array(16);
const signature = subtle.sign({ name: "Ed25519" }, pair.privateKey, data);

[
  subtle.sign({ name: "Ed25519" }, pair.publicKey, data),
  subtle.verify({ name: "Ed25519" }, pair.privateKey, signature, data),
  subtle.importKey(
    "jwk",
    { kty: "OKP", crv: "X25519", x: "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" },
    { name: "Ed25519" },
    true,
    ["verify"],
  ),
  subtle.importKey(
    "raw",
    new Uint8Array(16),
    { name: "Ed25519" },
    true,
    ["verify"],
  ),
];

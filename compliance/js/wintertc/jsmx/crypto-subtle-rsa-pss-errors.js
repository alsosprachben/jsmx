// Repo-authored WinterTC RSA-PSS rejection fixture.
const subtle = globalThis.crypto.subtle;

const pair = subtle.generateKey(
  {
    name: "RSA-PSS",
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: { name: "SHA-256" },
  },
  true,
  ["sign", "verify"],
);
const data = new Uint8Array(16);
const signature = subtle.sign(
  { name: "RSA-PSS", saltLength: 32 },
  pair.privateKey,
  data,
);

[
  subtle.generateKey(
    {
      name: "RSA-PSS",
      modulusLength: 1024,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
      hash: { name: "SHA-256" },
    },
    true,
    ["sign", "verify"],
  ),
  subtle.sign(
    { name: "RSA-PSS", saltLength: 32 },
    pair.publicKey,
    data,
  ),
  subtle.verify(
    { name: "RSA-PSS", saltLength: 32 },
    pair.privateKey,
    signature,
    data,
  ),
  subtle.importKey(
    "jwk",
    { kty: "oct", n: "AQAB", e: "AQAB" },
    {
      name: "RSA-PSS",
      modulusLength: 2048,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
      hash: { name: "SHA-256" },
    },
    true,
    ["verify"],
  ),
];

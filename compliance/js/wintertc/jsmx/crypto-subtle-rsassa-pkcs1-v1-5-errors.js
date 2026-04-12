// Repo-authored WinterTC RSASSA-PKCS1-v1_5 rejection fixture.
const subtle = globalThis.crypto.subtle;

const pair = subtle.generateKey(
  {
    name: "RSASSA-PKCS1-v1_5",
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: { name: "SHA-256" },
  },
  true,
  ["sign", "verify"],
);
const data = new Uint8Array(16);
const signature = subtle.sign(
  { name: "RSASSA-PKCS1-v1_5" },
  pair.privateKey,
  data,
);

[
  subtle.generateKey(
    {
      name: "RSASSA-PKCS1-v1_5",
      modulusLength: 1024,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
      hash: { name: "SHA-256" },
    },
    true,
    ["sign", "verify"],
  ),
  subtle.sign(
    { name: "RSASSA-PKCS1-v1_5" },
    pair.publicKey,
    data,
  ),
  subtle.verify(
    { name: "RSASSA-PKCS1-v1_5" },
    pair.privateKey,
    signature,
    data,
  ),
  subtle.importKey(
    "jwk",
    { kty: "oct", n: "AQAB", e: "AQAB" },
    {
      name: "RSASSA-PKCS1-v1_5",
      modulusLength: 2048,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
      hash: { name: "SHA-256" },
    },
    true,
    ["verify"],
  ),
];

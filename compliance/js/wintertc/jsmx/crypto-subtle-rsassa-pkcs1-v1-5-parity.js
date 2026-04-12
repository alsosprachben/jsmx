// Repo-authored WinterTC RSASSA-PKCS1-v1_5 parity fixture.
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
const verified = subtle.verify(
  { name: "RSASSA-PKCS1-v1_5" },
  pair.publicKey,
  signature,
  data,
);

const jwkPublic = subtle.exportKey("jwk", pair.publicKey);
const jwkPrivate = subtle.exportKey("jwk", pair.privateKey);
const reimportedPublic = subtle.importKey(
  "jwk",
  jwkPublic,
  {
    name: "RSASSA-PKCS1-v1_5",
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: { name: "SHA-256" },
  },
  true,
  ["verify"],
);
const reverified = subtle.verify(
  { name: "RSASSA-PKCS1-v1_5" },
  reimportedPublic,
  signature,
  data,
);

[pair, signature, verified, jwkPublic, jwkPrivate, reverified];

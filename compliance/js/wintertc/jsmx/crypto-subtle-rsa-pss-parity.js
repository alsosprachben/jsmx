// Repo-authored WinterTC RSA-PSS parity fixture.
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
const verified = subtle.verify(
  { name: "RSA-PSS", saltLength: 32 },
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
    name: "RSA-PSS",
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: { name: "SHA-256" },
  },
  true,
  ["verify"],
);
const reverified = subtle.verify(
  { name: "RSA-PSS", saltLength: 32 },
  reimportedPublic,
  signature,
  data,
);

const spkiPublic = subtle.exportKey("spki", pair.publicKey);
const pkcs8Private = subtle.exportKey("pkcs8", pair.privateKey);
const spkiReimport = subtle.importKey(
  "spki",
  spkiPublic,
  {
    name: "RSA-PSS",
    modulusLength: 2048,
    publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
    hash: { name: "SHA-256" },
  },
  true,
  ["verify"],
);
const spkiReverify = subtle.verify(
  { name: "RSA-PSS", saltLength: 32 },
  spkiReimport,
  signature,
  data,
);

[pair, signature, verified, jwkPublic, jwkPrivate, reverified,
 spkiPublic, pkcs8Private, spkiReverify];

// Repo-authored WinterTC Ed25519 parity fixture.
const subtle = globalThis.crypto.subtle;

const pair = subtle.generateKey(
  { name: "Ed25519" },
  true,
  ["sign", "verify"],
);

const data = new Uint8Array(16);
const signature = subtle.sign({ name: "Ed25519" }, pair.privateKey, data);
const verified = subtle.verify(
  { name: "Ed25519" },
  pair.publicKey,
  signature,
  data,
);

const rawPublic = subtle.exportKey("raw", pair.publicKey);
const jwkPublic = subtle.exportKey("jwk", pair.publicKey);
const jwkPrivate = subtle.exportKey("jwk", pair.privateKey);
const spkiPublic = subtle.exportKey("spki", pair.publicKey);
const pkcs8Private = subtle.exportKey("pkcs8", pair.privateKey);

const spkiReimport = subtle.importKey(
  "spki",
  spkiPublic,
  { name: "Ed25519" },
  true,
  ["verify"],
);
const spkiReverify = subtle.verify(
  { name: "Ed25519" },
  spkiReimport,
  signature,
  data,
);

[pair, signature, verified, rawPublic, jwkPublic, jwkPrivate,
 spkiPublic, pkcs8Private, spkiReverify];

// Repo-authored WinterTC ECDSA P-256 parity fixture.
const subtle = globalThis.crypto.subtle;

const pair = subtle.generateKey(
  { name: "ECDSA", namedCurve: "P-256" },
  true,
  ["sign", "verify"],
);

const data = new Uint8Array(16);
const signature = subtle.sign(
  { name: "ECDSA", hash: "SHA-256" },
  pair.privateKey,
  data,
);
const verified = subtle.verify(
  { name: "ECDSA", hash: "SHA-256" },
  pair.publicKey,
  signature,
  data,
);

const rawPublic = subtle.exportKey("raw", pair.publicKey);
const jwkPublic = subtle.exportKey("jwk", pair.publicKey);
const jwkPrivate = subtle.exportKey("jwk", pair.privateKey);

const reimportedPublic = subtle.importKey(
  "jwk",
  jwkPublic,
  { name: "ECDSA", namedCurve: "P-256" },
  true,
  ["verify"],
);
const reverified = subtle.verify(
  { name: "ECDSA", hash: "SHA-256" },
  reimportedPublic,
  signature,
  data,
);

const spkiPublic = subtle.exportKey("spki", pair.publicKey);
const pkcs8Private = subtle.exportKey("pkcs8", pair.privateKey);
const spkiReimport = subtle.importKey(
  "spki",
  spkiPublic,
  { name: "ECDSA", namedCurve: "P-256" },
  true,
  ["verify"],
);
const spkiReverify = subtle.verify(
  { name: "ECDSA", hash: "SHA-256" },
  spkiReimport,
  signature,
  data,
);

[pair, signature, verified, rawPublic, jwkPublic, jwkPrivate, reverified,
 spkiPublic, pkcs8Private, spkiReverify];

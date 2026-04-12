// Repo-authored WinterTC ECDSA P-256 rejection fixture.
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

[
  subtle.generateKey(
    { name: "ECDSA", namedCurve: "P-521" },
    true,
    ["sign", "verify"],
  ),
  subtle.sign(
    { name: "ECDSA", hash: "SHA-256" },
    pair.publicKey,
    data,
  ),
  subtle.verify(
    { name: "ECDSA", hash: "SHA-256" },
    pair.privateKey,
    signature,
    data,
  ),
  subtle.importKey(
    "jwk",
    { kty: "EC", crv: "P-384", x: "AAAA", y: "AAAA" },
    { name: "ECDSA", namedCurve: "P-256" },
    true,
    ["verify"],
  ),
];

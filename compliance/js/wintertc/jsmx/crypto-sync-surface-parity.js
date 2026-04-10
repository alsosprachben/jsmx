// Repo-authored WinterTC sync crypto parity fixture.
const cryptoObject = globalThis.crypto;
const subtleA = cryptoObject.subtle;
const subtleB = cryptoObject.subtle;

[
  typeof cryptoObject,
  typeof subtleA,
  subtleA === subtleB,
];

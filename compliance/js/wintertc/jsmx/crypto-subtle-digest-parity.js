// Repo-authored WinterTC subtle.digest parity fixture.
const subtle = globalThis.crypto.subtle;
const input = new Uint8Array(16);
const promiseA = subtle.digest("SHA-256", input);
const promiseB = subtle.digest({ name: "SHA-1" }, input.buffer);

[promiseA, promiseB];

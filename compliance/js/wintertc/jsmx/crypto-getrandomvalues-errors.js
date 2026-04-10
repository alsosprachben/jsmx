// Repo-authored WinterTC getRandomValues error parity fixture.
let floatName = "";
let quotaName = "";

try {
  crypto.getRandomValues(new Float32Array(4));
} catch (error) {
  floatName = error.name;
}

try {
  crypto.getRandomValues(new Uint8Array(65537));
} catch (error) {
  quotaName = error.name;
}

[
  floatName,
  quotaName,
];

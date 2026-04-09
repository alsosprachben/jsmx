// Repo-authored idiomatic lowering fixture for explicit BigInt arithmetic helpers.

const left = 12345678901234567890n;
const right = 9876543210n;

if (left + right !== 12345678911111111100n) {
  throw new Error("expected bigint addition to preserve multi-limb precision");
}

if (left - right !== 12345678891358024680n) {
  throw new Error("expected bigint subtraction to preserve multi-limb precision");
}

if (left * right !== 121932631124828532111263526900n) {
  throw new Error("expected bigint multiplication to preserve multi-limb precision");
}

if (-left !== -12345678901234567890n) {
  throw new Error("expected unary negation to preserve bigint magnitude");
}

let threw = false;
try {
  left + 1;
} catch (err) {
  if (!(err instanceof TypeError)) {
    throw err;
  }
  threw = true;
}

if (!threw) {
  throw new Error("expected mixed Number/BigInt arithmetic to keep throwing");
}

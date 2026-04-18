// Repo-authored BigInt 64-bit construction parity fixture.
//
// BigInt round-trips values across the full 64-bit signed
// and unsigned ranges — these are the natural C constructor
// entry points, and they extend past IEEE-754's exact-integer
// range (2^53).

// Zero is falsy; String(0n) === "0"; typeof is "bigint".
const zero = 0n;

// Positive small.
const forty_two = 42n;
// String(42n) === "42"

// Full int64 range: INT64_MIN = -9223372036854775808n.
// This is past Number's safe-integer range (2^53).
const i64_min = -9223372036854775808n;

// Full uint64 range: UINT64_MAX = 18446744073709551615n.
// Serializes to a 20-digit decimal.
const u64_max = 18446744073709551615n;

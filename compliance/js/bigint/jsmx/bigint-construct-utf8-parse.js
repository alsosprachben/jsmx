// Repo-authored BigInt UTF-8 parse/round-trip fixture.
//
// BigInt can be constructed from a decimal string via the C
// jsval_bigint_new_utf8 entry point. Parsed values normalize
// (leading zeros, "-0" → "0"), round-trip through
// jsval_bigint_copy_utf8, and support arbitrary precision
// beyond i64.

// Zero normalizes across construction paths.
const zero_plain = 0n;
const zero_from_string = BigInt("-0000");
// Both serialize to "0"; jsval_strict_eq(zero_plain, zero_from_string) === 1

// Past 2^63 — only string construction reaches this range.
const big_positive = 12345678901234567890n;
// String(big_positive) === "12345678901234567890"

// Negative counterpart; equals -big_positive.
const big_negative = -12345678901234567890n;

// BigInt does NOT coerce to Number: jsval_to_number rejects
// with errno == ENOTSUP. Observable via Number(zero_plain) in
// JS (which throws TypeError), but the C path is -1/ENOTSUP.

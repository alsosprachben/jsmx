// Repo-authored BigInt comparison-ordering parity fixture.
//
// jsval_bigint_compare produces memcmp-style sign across
// zero, small, large-positive, large-negative, and equal
// values. Ordering survives past the IEEE-754 safe-integer
// range where Number comparison would lose precision.

const zero = 0n;
const forty_two = 42n;
const forty_two_text = BigInt("42");
const big_positive = 12345678901234567890n;
const big_negative = -12345678901234567890n;

// zero < forty_two: compare(zero, forty_two) < 0
// forty_two > zero: compare(forty_two, zero) > 0
// forty_two == forty_two_text: compare(... , ...) === 0
// big_negative < big_positive: compare(..., ...) < 0
// big_positive == big_positive: reflexive 0

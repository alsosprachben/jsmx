// Repo-authored BigInt loose-equality + Number-coercion
// fixture.
//
// BigInt == Number compares by mathematical value (loose
// equality), while BigInt === Number is always false.
// BigInt == "decimal-string" coerces the string. BigInt
// arithmetic does NOT mix with Number — jsval_add returns
// -1/ENOTSUP.

const forty_two = 42n;

// 42n == 42.0 → true (loose: same mathematical value).
// 42n == 42.5 → false.
// 42n == "42" → true (string coerces to BigInt-equal value).
// 42n === 42.0 → false (strict: different types).

// 42n + 1 throws TypeError in JS. C surface: jsval_add(bi,
// num) returns -1 with errno == ENOTSUP.

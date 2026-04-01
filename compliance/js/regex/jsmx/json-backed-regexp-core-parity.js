// Repo-authored parity fixture for the jsval regex core bridge.
//
// This covers two jsmx-specific contracts that the upstream suite does not
// express directly:
// 1. parsed JSON-backed string receivers behave like native strings for
//    RegExp.prototype.test-style execution
// 2. regex state helpers expose stable semantic source, per-flag accessor,
//    constructor-copy, flag, and lastIndex behavior for generated C

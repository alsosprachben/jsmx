// Repo-authored parity fixture for repeated regexp execution state.
//
// This covers the jsmx-specific contract that the upstream suite does not
// express directly: parsed JSON-backed string receivers must behave like
// native strings across repeated global exec state transitions, miss-reset
// behavior, and non-global lastIndex preservation.

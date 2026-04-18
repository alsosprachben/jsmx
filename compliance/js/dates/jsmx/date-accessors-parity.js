// Repo-authored Date UTC accessors parity fixture.
//
// Given a Date constructed from a fixed UTC time value, every
// UTC getter must return the spec-aligned component. Local-time
// accessors are intentionally excluded (TZ-dependent, would
// make the fixture flaky).

const d = new Date(1704112245500);  // 2024-01-01T12:30:45.500Z

// d.getUTCFullYear()     === 2024
// d.getUTCMonth()        === 0
// d.getUTCDate()         === 1
// d.getUTCDay()          === 1 (Monday)
// d.getUTCHours()        === 12
// d.getUTCMinutes()      === 30
// d.getUTCSeconds()      === 45
// d.getUTCMilliseconds() === 500

// setUTCFullYear round-trip
d.setUTCFullYear(2025);
// d.getUTCFullYear() === 2025

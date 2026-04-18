// Repo-authored Date construction parity fixture.
//
// Three deterministic construction paths produce a valid Date
// with the expected time value. `new Date()` (non-deterministic)
// is deliberately not covered.

// Unix epoch via time value.
const epoch_via_time = new Date(0);
// epoch_via_time.getTime() === 0; .valueOf() === 0

// Unix epoch via ISO.
const epoch_via_iso = new Date("1970-01-01T00:00:00.000Z");
// epoch_via_iso.getTime() === 0

// 2024-01-01T12:30:45.500Z via UTC fields.
const via_fields = new Date(Date.UTC(2024, 0, 1, 12, 30, 45, 500));
// via_fields.getTime() === 1704112245500

// All three are valid.

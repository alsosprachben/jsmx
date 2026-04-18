// Repo-authored Date ISO serialization parity fixture.
//
// Date.toISOString() produces the canonical
// "YYYY-MM-DDTHH:mm:ss.sssZ" form. Date.toJSON() returns the
// same value. Parse → serialize → parse round-trips byte-
// identically for canonical inputs.

const d1 = new Date("2024-01-01T12:30:45.500Z");
// d1.toISOString() === "2024-01-01T12:30:45.500Z"
// d1.toJSON()      === "2024-01-01T12:30:45.500Z"

const d2 = new Date(1704112245500);
// d2.toISOString() === "2024-01-01T12:30:45.500Z"

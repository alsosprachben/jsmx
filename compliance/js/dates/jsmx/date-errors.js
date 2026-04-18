// Repo-authored Date error-path fixture.
//
// Spec behavior for invalid Dates: construction never throws
// but isValid returns false (JS: getTime() returns NaN).
// toISOString / toJSON throw on an invalid Date (spec:
// RangeError). jsmx surfaces this with -1.

// Malformed ISO input: spec allows either throwing or
// constructing an Invalid Date. jsmx's strict C API path
// (jsval_date_new_iso) returns -1.
//
// JS usage: `new Date("not a date")` is not rejected but
// yields a Date whose getTime() is NaN; toISOString() on
// that throws RangeError. jsmx surfaces the equivalent path
// with -1 from jsval_date_new_iso on the malformed input.

// NaN time value: permissive — constructs a valid struct with
// is_valid == 0.
const bad_time = new Date(NaN);
// isNaN(bad_time.getTime()) === true

// toISOString on invalid Date throws RangeError.
let threw_iso = false;
try { bad_time.toISOString(); } catch (e) { threw_iso = true; }
// threw_iso === true

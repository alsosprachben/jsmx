// Repo-authored JSON parity fixture.
//
// JSON.parse / JSON.stringify round-trip for canonical
// structures. Exercises jsval_json_parse → accessor queries →
// jsval_copy_json serialization.

// Empty object stays empty.
const empty_obj = JSON.parse("{}");
// JSON.stringify(empty_obj) === "{}"

// Empty array stays empty.
const empty_arr = JSON.parse("[]");
// JSON.stringify(empty_arr) === "[]"

// Nested mixed leaves preserved across round trip.
const nested = JSON.parse('{"a":1,"b":[2,true,null]}');
// nested.a === 1
// nested.b[0] === 2
// nested.b[1] === true
// nested.b[2] === null
// JSON.stringify(nested) === '{"a":1,"b":[2,true,null]}'

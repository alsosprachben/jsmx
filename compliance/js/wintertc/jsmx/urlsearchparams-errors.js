// Repo-authored WinterTC URLSearchParams error-path fixture.
//
// Exercises jsval_url_search_params_* negative paths. jsmx
// surfaces these as a return code + errno, paralleling the
// error shape of the URL and fetch error fixtures.

const params = new URLSearchParams("a=1");

// get on missing name → null (not an error, just "no value").
// Tests assert this null shape here rather than in the parity
// fixture to keep null-vs-missing semantics explicit.
// params.get("missing") === null

// Calling URLSearchParams methods with a wrong-kind receiver is
// a spec TypeError; the C API signals it with a -1 return. The
// fixture verifies jsval_url_search_params_get and _size reject
// non-URLSearchParams inputs.

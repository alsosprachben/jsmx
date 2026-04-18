// Repo-authored JSON error-path fixture.
//
// Parse rejects malformed input; stringify rejects values that
// have no JSON representation (undefined, NaN) and circular
// structures.

// Parse malformed input.
let threw_open = false;
try { JSON.parse("{"); } catch (e) { threw_open = true; }

let threw_unterminated_string = false;
try { JSON.parse('"hello'); } catch (e) { threw_unterminated_string = true; }

// Stringify undefined — jsmx surfaces as ENOTSUP (spec returns
// undefined; C API is stricter since the caller expects bytes).
let threw_undef = false;
try { JSON.stringify(undefined); } catch (e) { threw_undef = true; }

// Stringify NaN — jsmx rejects with ENOTSUP (spec writes "null").
let threw_nan = false;
try { JSON.stringify(NaN); } catch (e) { threw_nan = true; }

// Circular reference rejected (jsmx: ELOOP; spec: TypeError).
const cycle = {};
cycle.self = cycle;
let threw_cycle = false;
try { JSON.stringify(cycle); } catch (e) { threw_cycle = true; }

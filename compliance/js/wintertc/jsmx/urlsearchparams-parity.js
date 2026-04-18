// Repo-authored WinterTC URLSearchParams parity fixture.
//
// Exercises the WHATWG URLSearchParams surface: construct from
// a wire-form query string, all accessor and mutation methods,
// and round-trip serialization. Percent-decoding of %20 and
// multi-value ordering are the primary spec behaviors checked.

const params = new URLSearchParams("a=1&b=two%20words&a=3");

// Accessors
// params.size              === 3
// params.has("a")          === true
// params.has("missing")    === false
// params.get("a")          === "1"       (first match wins)
// params.getAll("a")       === ["1", "3"]
// params.get("b")          === "two words"

// Mutation
params.append("c", "new");      // size → 4
params.set("a", "sole");        // removes all a's, appends one
params.delete("b");             // removes b entirely

// After mutations:
//   keys in any order: ["a", "c"]; size === 2

// Stable sort by name (preserves order within same name)
params.sort();

// toString produces wire form with percent-encoding
// for reserved chars. After sort + our mutations:
//   params.toString() === "a=sole&c=new"

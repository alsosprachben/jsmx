// Repo-authored idiomatic lowering fixture for JSON-backed/native bitwise parity.

const parsed = JSON.parse('{"truth":true,"zero":"0","one":"1","bad":"x","nothing":null,"num":5}');

if ((~parsed.nothing) !== -1) {
  throw new Error("expected parsed null bitwise-not to coerce to -1");
}

if ((parsed.truth & parsed.num) !== 1) {
  throw new Error("expected parsed truth & parsed num to preserve bitwise-and");
}

if ((parsed.one | false) !== 1) {
  throw new Error("expected parsed one | false to preserve bitwise-or");
}

if ((parsed.bad ^ true) !== 1) {
  throw new Error("expected parsed bad ^ true to preserve bitwise-xor after numeric coercion");
}

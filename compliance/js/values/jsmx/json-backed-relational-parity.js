// Repo-authored idiomatic lowering fixture for JSON-backed/native relational parity.

const parsed = JSON.parse('{"num":2,"flag":true,"nothing":null,"text":"1","bad":"x","left":"1","right":"2"}');

if (!(parsed.num > parsed.flag)) {
  throw new Error("expected parsed number > parsed true to preserve numeric relational behavior");
}

if (!(parsed.nothing < parsed.flag)) {
  throw new Error("expected parsed null < parsed true to preserve numeric relational behavior");
}

if ((parsed.text < parsed.flag) !== false) {
  throw new Error('expected parsed "1" < parsed true to preserve numeric coercion behavior');
}

if (!(parsed.flag >= parsed.text)) {
  throw new Error('expected parsed true >= parsed "1" to preserve numeric coercion behavior');
}

if ((parsed.bad < 1) !== false) {
  throw new Error('expected parsed invalid numeric text < 1 to stay false via NaN');
}

// The translator should not flatten `parsed.left < parsed.right` through the
// current jsmx relational layer yet, because both operands are strings.

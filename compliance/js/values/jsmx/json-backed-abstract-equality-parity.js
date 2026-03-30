// Repo-authored idiomatic lowering fixture for JSON-backed/native abstract equality parity.

const parsed = JSON.parse('{"num":1,"flag":true,"text":"1","empty":"","nothing":null}');

if (!(parsed.num == "1")) {
  throw new Error('expected parsed number == "1" through primitive coercion');
}

if (!(parsed.flag == "1")) {
  throw new Error('expected parsed true == "1" through primitive coercion');
}

if (!(parsed.text == 1)) {
  throw new Error('expected parsed string == 1 through numeric coercion');
}

if (!(parsed.empty == 0)) {
  throw new Error('expected parsed empty string == 0');
}

if (!(parsed.nothing == undefined)) {
  throw new Error('expected parsed null == undefined');
}

if (!(parsed.nothing != 0)) {
  throw new Error('expected parsed null != 0');
}

// Repo-authored idiomatic lowering fixture for repeated shallow own-property
// copy in Object.assign-style left-to-right source order.

const dst = {};
const first = { a: 1, b: 2 };
const second = JSON.parse('{"b":9,"c":3}');

Object.assign(dst, first, second);

if (JSON.stringify(dst) !== '{"a":1,"b":9,"c":3}') {
  throw new Error("expected repeated own-property copy to preserve overwrite and append order");
}

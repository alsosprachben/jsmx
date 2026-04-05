// Repo-authored idiomatic lowering fixture for shallow-promoting a parsed array
// before dense front mutation through shift.

const arr = JSON.parse("[4,5,6]");

if (arr[0] !== 4) {
  throw new Error("expected parsed arr[0] to read before promotion");
}

arr.shift();

if (JSON.stringify(arr) !== "[5,6]") {
  throw new Error("expected promoted shift to remove the parsed dense head");
}

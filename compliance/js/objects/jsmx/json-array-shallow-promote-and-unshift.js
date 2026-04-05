// Repo-authored idiomatic lowering fixture for shallow-promoting a parsed array
// before dense front mutation through unshift.

const arr = JSON.parse("[4,5]");

if (arr[1] !== 5) {
  throw new Error("expected parsed arr[1] to read before promotion");
}

arr.unshift(7);

if (JSON.stringify(arr) !== "[7,4,5]") {
  throw new Error("expected promoted unshift to prepend the parsed dense array");
}

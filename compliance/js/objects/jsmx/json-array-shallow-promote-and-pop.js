// Repo-authored idiomatic lowering fixture for shallow-promoting a parsed array
// before dense tail mutation through pop.

const arr = JSON.parse("[4,5,6]");

if (arr[2] !== 6) {
  throw new Error("expected parsed arr[2] to read before promotion");
}

arr.pop();

if (JSON.stringify(arr) !== "[4,5]") {
  throw new Error("expected promoted pop to remove the parsed dense tail");
}

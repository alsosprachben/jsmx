// Repo-authored idiomatic lowering fixture for dense native array pop.

const arr = [1, 2];

const first = arr.pop();
if (first !== 2) {
  throw new Error("expected first pop to return the former tail value");
}

arr.push(9);

const second = arr.pop();
if (second !== 9) {
  throw new Error("expected push followed by pop to return the appended value");
}

if (JSON.stringify(arr) !== "[1]") {
  throw new Error("expected pop to shrink the observable dense tail");
}

// Repo-authored idiomatic lowering fixture for ternary container identity parity.

const parsed = JSON.parse('{"obj":{},"arr":[]}');
const nativeObj = {};
const otherObj = {};
const nativeArr = [];
const otherArr = [];

if ((parsed.obj ? parsed.obj : otherObj) !== parsed.obj) {
  throw new Error("expected parsed object truthiness to preserve object identity");
}

if ((parsed.arr ? parsed.arr : otherArr) !== parsed.arr) {
  throw new Error("expected parsed array truthiness to preserve array identity");
}

if ((nativeObj ? nativeObj : otherObj) !== nativeObj) {
  throw new Error("expected native object truthiness to preserve object identity");
}

if ((nativeArr ? nativeArr : otherArr) !== nativeArr) {
  throw new Error("expected native array truthiness to preserve array identity");
}

if ((undefined ? nativeObj : otherObj) !== otherObj) {
  throw new Error("expected undefined to take the ternary else branch");
}

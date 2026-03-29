// Repo-authored idiomatic lowering fixture for container identity and
// truthiness.

const nativeObj = {};
const sameNativeObj = nativeObj;
const otherNativeObj = {};
const nativeArr = [];
const sameNativeArr = nativeArr;
const otherNativeArr = [];
const parsed = JSON.parse('{"obj":{},"obj2":{},"arr":[],"arr2":[]}');

if (!nativeObj || !nativeArr || !parsed.obj || !parsed.arr) {
  throw new Error("expected object and array values to stay truthy");
}

if (nativeObj !== sameNativeObj || nativeArr !== sameNativeArr) {
  throw new Error("expected the same native container reference to stay strictly equal");
}

if (nativeObj === otherNativeObj || nativeArr === otherNativeArr) {
  throw new Error("expected distinct native containers to fail strict equality");
}

if (parsed.obj !== parsed.obj || parsed.arr !== parsed.arr) {
  throw new Error("expected repeated reads of the same parsed child to stay strictly equal");
}

if (parsed.obj === parsed.obj2 || parsed.arr === parsed.arr2) {
  throw new Error("expected distinct parsed children to fail strict equality");
}

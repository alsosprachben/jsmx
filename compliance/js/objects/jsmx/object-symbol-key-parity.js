// Repo-authored idiomatic lowering fixture for symbol-keyed native object own
// properties.

const a = Symbol("token");
const b = Symbol("token");
const src = { [a]: 7, plain: true, [b]: 9 };
const clone = { ...src };
const copied = Object.assign({}, src);

if (clone[a] !== 7 || clone[b] !== 9 || copied[a] !== 7 || copied[b] !== 9) {
  throw new Error("expected clone and copy to preserve symbol-keyed own properties");
}

if (JSON.stringify(src) !== '{"plain":true}') {
  throw new Error("expected JSON emission to omit symbol-keyed properties");
}

const keys = Reflect.ownKeys(src);
if (keys[0] !== a || keys[1] !== "plain" || keys[2] !== b) {
  throw new Error("expected own-key order to include symbols in insertion order");
}

delete src[a];
const afterDelete = Reflect.ownKeys(src);
if (afterDelete[0] !== "plain" || afterDelete[1] !== b) {
  throw new Error("expected symbol-key delete to compact later own keys");
}

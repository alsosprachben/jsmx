// Repo-authored idiomatic lowering fixture for native own-property checks and
// object deletion.

const obj = { keep: true, drop: 7 };

if (!Object.prototype.hasOwnProperty.call(obj, "keep")) {
  throw new Error("expected keep to be an own property");
}

if (!Object.prototype.hasOwnProperty.call(obj, "drop")) {
  throw new Error("expected drop to be an own property");
}

if (Object.prototype.hasOwnProperty.call(obj, "missing")) {
  throw new Error("missing should not be reported as an own property");
}

delete obj.drop;

if (Object.prototype.hasOwnProperty.call(obj, "drop")) {
  throw new Error("expected deleted property to disappear from own-property checks");
}

if (JSON.stringify(obj) !== '{"keep":true}') {
  throw new Error("expected deletion to be reflected in emitted JSON");
}

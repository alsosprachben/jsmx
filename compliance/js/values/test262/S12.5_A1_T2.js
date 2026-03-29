// Simplified vendored test262-style truthiness case for object and array
// operands.

var obj = {};
var arr = [];

if (!obj) {
  throw new Error("expected object values to be truthy");
}

if (!arr) {
  throw new Error("expected array values to be truthy");
}

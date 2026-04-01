// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.split
description: Split using a variety of zero-width and consuming RegExp separators
info: |
  String.prototype.split ( separator, limit )
  ...
  RegExp.prototype [ @@split ] ( string, limit )
includes: [compareArray.js]
---*/

assert.compareArray("x".split(/^/), ["x"], '"x".split(/^/) must return ["x"]');
assert.compareArray("x".split(/$/), ["x"], '"x".split(/$/) must return ["x"]');
assert.compareArray("x".split(/.?/), ["", ""], '"x".split(/.?/) must return ["", ""]');
assert.compareArray("x".split(/.*?/), ["x"], '"x".split(/.*?/) must return ["x"]');
assert.compareArray("x".split(/(?:)/), ["x"], '"x".split(/(?:)/) must return ["x"]');
assert.compareArray("x".split(/[^]/), ["", ""], '"x".split(/[^]/) must return ["", ""]');

// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Honors initial value of the `lastIndex` property
es6id: 21.2.5.2
---*/

var r = /./y;
var match;
r.lastIndex = 1;

match = r.exec('abc');

assert(match !== null);
assert.sameValue(match.length, 1);
assert.sameValue(match[0], 'b');

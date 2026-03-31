// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Resets the `lastIndex` property to zero after a match failure
es6id: 21.2.5.2
---*/

var r = /c/y;
r.lastIndex = 1;

r.exec('abc');

assert.sameValue(r.lastIndex, 0);

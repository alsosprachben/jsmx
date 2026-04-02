// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-string.prototype.replaceall
description: >
  Matching empty string for the this value and the searchValue
info: |
  String.prototype.replaceAll ( searchValue, replaceValue )

  ...
  10. Let position be ! StringIndexOf(string, searchString, 0).
  11. Repeat, while position is not -1
    a. Append position to the end of matchPositions.
    b. Let position be ! StringIndexOf(string, searchString, position + advanceBy).
features: [String.prototype.replaceAll]
---*/

var result = ''.replaceAll('', 'abc');
assert.sameValue(result, 'abc');

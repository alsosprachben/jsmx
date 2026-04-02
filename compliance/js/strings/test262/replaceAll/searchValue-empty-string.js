// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-string.prototype.replaceall
description: >
  Replacements when the search value is the empty string
info: |
  String.prototype.replaceAll ( searchValue, replaceValue )

  10. Let position be ! StringIndexOf(string, searchString, 0).
  11. Repeat, while position is not -1
    a. Append position to the end of matchPositions.
    b. Let position be ! StringIndexOf(string, searchString, position + advanceBy).
  ...
  14. For each position in matchPositions, do
    a. If functionalReplace is true, then
      ...
    b. Else,
      ...
      ii. Let captures be a new empty List.
      iii. Let replacement be GetSubstitution(searchString, string, position, captures, undefined, replaceValue).
features: [String.prototype.replaceAll]
---*/

var result;

result = 'aab c  \nx'.replaceAll('', '_');
assert.sameValue(result, '_a_a_b_ _c_ _ _\n_x_');

result = 'a'.replaceAll('', '_');
assert.sameValue(result, '_a_');

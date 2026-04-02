// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-string.prototype.replaceall
description: >
  Replacement Text Symbol Substitutions ($N)
info: |
  String.prototype.replaceAll ( searchValue, replaceValue )
features: [String.prototype.replaceAll, Symbol.replace]
---*/

var str = 'ABC AAA ABC AAA';

var result;

result = str.replaceAll('ABC', '$1');
assert.sameValue(result, '$1 AAA $1 AAA');

result = str.replaceAll('ABC', '$2');
assert.sameValue(result, '$2 AAA $2 AAA');

result = str.replaceAll('ABC', '$3');
assert.sameValue(result, '$3 AAA $3 AAA');

result = str.replaceAll('ABC', '$4');
assert.sameValue(result, '$4 AAA $4 AAA');

result = str.replaceAll('ABC', '$5');
assert.sameValue(result, '$5 AAA $5 AAA');

result = str.replaceAll('ABC', '$6');
assert.sameValue(result, '$6 AAA $6 AAA');

result = str.replaceAll('ABC', '$7');
assert.sameValue(result, '$7 AAA $7 AAA');

result = str.replaceAll('ABC', '$8');
assert.sameValue(result, '$8 AAA $8 AAA');

result = str.replaceAll('ABC', '$9');
assert.sameValue(result, '$9 AAA $9 AAA');

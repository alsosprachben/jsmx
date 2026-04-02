// Copyright (C) 2019 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-string.prototype.replaceall
description: >
  Replacement Text Symbol Substitutions ($$)
info: |
  String.prototype.replaceAll ( searchValue, replaceValue )
features: [String.prototype.replaceAll]
---*/

var str = 'Ninguém é igual a ninguém. Todo o ser humano é um estranho ímpar.';

var result;

result = str.replaceAll('ninguém', '$$');
assert.sameValue(result, 'Ninguém é igual a $. Todo o ser humano é um estranho ímpar.');

result = str.replaceAll('é', '$$');
assert.sameValue(result, 'Ningu$m $ igual a ningu$m. Todo o ser humano $ um estranho ímpar.');

result = str.replaceAll('é', '$$ -');
assert.sameValue(result, 'Ningu$ -m $ - igual a ningu$ -m. Todo o ser humano $ - um estranho ímpar.');

result = str.replaceAll('é', '$$&');
assert.sameValue(result, 'Ningu$&m $& igual a ningu$&m. Todo o ser humano $& um estranho ímpar.');

result = str.replaceAll('é', '$$$');
assert.sameValue(result, 'Ningu$$m $$ igual a ningu$$m. Todo o ser humano $$ um estranho ímpar.');

result = str.replaceAll('é', '$$$$');
assert.sameValue(result, 'Ningu$$m $$ igual a ningu$$m. Todo o ser humano $$ um estranho ímpar.');

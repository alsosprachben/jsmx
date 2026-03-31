// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: match returns array as specified in 15.10.6.2
es5id: 15.5.4.10_A2_T10
description: Regular expression is /([\\d]{5})([-\\ ]?[\\d]{4})?$/
---*/

var __string = "Boston, MA 02134";
var __matches = ["02134", "02134", undefined];
var __re = /([\d]{5})([-\ ]?[\d]{4})?$/;
__re.lastIndex = __string.lastIndexOf("0");

if (__string.match(__re).length !== 3) throw new Test262Error();
if (__string.match(__re).index !== __string.lastIndexOf("0")) throw new Test262Error();
for (var mi = 0; mi < __matches.length; mi++) {
  if (__string.match(__re)[mi] !== __matches[mi]) throw new Test262Error();
}

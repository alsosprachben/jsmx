// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Advancement of lastIndex
es6id: 21.2.5.2.2
---*/

assert.sameValue(/\udf06/u.exec('\ud834\udf06'), null);

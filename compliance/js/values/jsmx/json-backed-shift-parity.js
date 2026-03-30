const parsed = JSON.parse('{"truth":true,"count":"33","neg":-1,"num":5}');

if ((parsed.truth << parsed.count) !== 2) throw new Error("truth << count");
if ((parsed.num >> parsed.count) !== 2) throw new Error("num >> count");
if ((parsed.neg >>> parsed.truth) !== 2147483647) throw new Error("neg >>> truth");

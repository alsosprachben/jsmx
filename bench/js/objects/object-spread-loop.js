const OUTER = 40000;
const secondJson = '{"drop":9,"tail":10}';

let checksum = 0;

for (let i = 0; i < OUTER; i++) {
  const first = { keep: true, drop: i & 7, items: [] };
  const second = JSON.parse(secondJson);
  const out = { head: i & 3, ...first, ...second, tail: i & 15 };

  checksum += out.head;
  checksum += out.drop;
  checksum += out.tail;
  checksum += Object.keys(out).length;
}

process.stdout.write(String(checksum) + "\n");

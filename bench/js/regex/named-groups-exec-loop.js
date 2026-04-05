const OUTER = 20000;
const inputs = [
  "alpha12 beta34 gamma56",
  "omega90 sigma12 rho34",
];

let checksum = 0;

for (let i = 0; i < OUTER; i++) {
  const text = inputs[i & 1];
  const regex = /(?<word>[a-z]+)(?<digits>\d+)/g;
  let match;

  while ((match = regex.exec(text)) !== null) {
    checksum += match[0].length;
    checksum += match.groups.word.length;
    checksum += Number(match.groups.digits);
  }
}

process.stdout.write(String(checksum) + "\n");

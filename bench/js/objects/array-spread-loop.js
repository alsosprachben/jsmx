const OUTER = 40000;
const secondJson = "[3,4]";

let checksum = 0;

for (let i = 0; i < OUTER; i++) {
  const first = [i & 7, 2];
  const out = [0, ...first, ...JSON.parse(secondJson), 9];

  checksum += out[0] + out[1] + out[2] + out[3] + out[4] + out[5];
}

process.stdout.write(String(checksum) + "\n");

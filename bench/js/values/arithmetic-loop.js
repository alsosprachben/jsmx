const OUTER = 250000;

let acc = 0;

for (let i = 0; i < OUTER; i++) {
  acc = (((acc + i) * 3) ^ (i >>> 3)) | 0;
}

process.stdout.write(String(acc) + "\n");

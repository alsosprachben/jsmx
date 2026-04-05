const OUTER = 30000;
const base = "alpha beta gamma";

let checksum = 0;

for (let i = 0; i < OUTER; i++) {
  const suffix = (i & 1) === 0 ? "-xy" : "-zz";
  const value = base.replaceAll("a", "ab").slice(1, 10).concat(suffix);

  checksum += value.length;
  checksum += value.charCodeAt(0);
  checksum += value.charCodeAt(value.length - 1);
}

process.stdout.write(String(checksum) + "\n");

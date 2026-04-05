// Repo-authored idiomatic lowering fixture for dense native array shift.

const arr = [1, 2];

const first = arr.shift();
if (first !== 1) {
  throw new Error("expected first shift to return the former head value");
}

arr.push(9);

const second = arr.shift();
if (second !== 2) {
  throw new Error("expected second shift to return the new dense head");
}

if (JSON.stringify(arr) !== "[9]") {
  throw new Error("expected shift to compact the remaining dense prefix");
}

// Repo-authored idiomatic lowering fixture for translator-planned capacity
// checks around shallow promotion and dense native arrays.

const root = JSON.parse('{"keep":7,"items":[1,2],"nested":{"flag":true}}');

root.keep = 9;
root.items[1] = 8;
root.items.push(3);

if (JSON.stringify(root) !== '{"keep":9,"items":[1,8,3],"nested":{"flag":true}}') {
  throw new Error("expected exact planned capacity to succeed once the translator chooses a large enough native container");
}

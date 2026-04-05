// Repo-authored idiomatic lowering fixture for insert-only and replace dense
// splice edits.

const insertTarget = [1, 4];
const insertRemoved = insertTarget.splice(1, 0, 7, 8);
const replaceTarget = [1, 2, 3, 4];
const replaceRemoved = replaceTarget.splice(1, 2, 9);

if (JSON.stringify({
  insertTarget,
  insertRemoved,
  replaceTarget,
  replaceRemoved
}) !== '{"insertTarget":[1,7,8,4],"insertRemoved":[],"replaceTarget":[1,9,4],"replaceRemoved":[2,3]}') {
  throw new Error("expected dense splice to support both insertion and replacement with removed results");
}

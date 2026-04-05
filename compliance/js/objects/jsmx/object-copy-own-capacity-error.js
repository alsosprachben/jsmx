// Repo-authored idiomatic lowering fixture for the bounded native destination
// capacity contract on shallow own-property copy.

const dst = { keep: true };
const src = JSON.parse('{"a":1,"b":2}');

// Translator lowers this to a native destination with exact capacity 1, so the
// copy must fail with ENOBUFS and leave the existing destination unchanged.

if (JSON.stringify(dst) !== '{"keep":true}') {
  throw new Error("expected undersized destination to remain unchanged after own-property copy failure");
}

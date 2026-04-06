const raw = process.argv[2] ?? "  world  ";
const mode = process.argv[3] ?? "plain";

const trimmed = raw.trim();
const phrase = trimmed.replaceAll("-", " ");
const prefix = phrase.startsWith("jsmx") ? "runtime" : "hello";
const suffix = mode === "excited" ? "!" : ".";

process.stdout.write(prefix + ", " + phrase + suffix + "\n");

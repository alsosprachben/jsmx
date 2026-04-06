const fs = require("fs");

function joinPath(base, name) {
  if (base === "/") {
    return `/${name}`;
  }
  return base.endsWith("/") ? base + name : `${base}/${name}`;
}

function kindFromStat(stat) {
  if (stat.isDirectory()) {
    return "directory";
  }
  if (stat.isFile()) {
    return "file";
  }
  return "other";
}

function classifyPathSync(path) {
  const lst = fs.lstatSync(path);
  const info = {
    kind: kindFromStat(lst),
    targetKind: kindFromStat(lst),
    isSymlink: lst.isSymbolicLink(),
  };

  if (info.isSymlink) {
    const st = fs.statSync(path);
    info.targetKind = kindFromStat(st);
  }
  return info;
}

function realpathSync(path) {
  return fs.realpathSync(path);
}

function listDirSync(path) {
  return fs
    .readdirSync(path)
    .map((name) => {
      const fullPath = joinPath(path, name);
      const info = classifyPathSync(fullPath);

      return {
        name,
        path: fullPath,
        kind: info.kind,
        targetKind: info.targetKind,
        isSymlink: info.isSymlink,
      };
    })
    .sort((a, b) => a.name.localeCompare(b.name));
}

function readTextFileSync(path) {
  return fs.readFileSync(path, "utf8");
}

module.exports = {
  classifyPathSync,
  joinPath,
  listDirSync,
  readTextFileSync,
  realpathSync,
};


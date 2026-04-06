const fs = require("fs");
const runtimeFs = require("../runtime_modules/node/fs_sync");

const PROGRAM = process.argv[1]
  ? process.argv[1].split(/[\\/]/).pop()
  : "grep.js";
const STDIN_LABEL = "(standard input)";

function usage() {
  process.stderr.write(
    `usage: ${PROGRAM} [-FivncqHhRrsx] [-e pattern] [pattern] [file ...]\n`,
  );
  process.exit(2);
}

function fail(message, exitCode = 2) {
  process.stderr.write(`${PROGRAM}: ${message}\n`);
  process.exit(exitCode);
}

function errorText(error) {
  if (!error || typeof error !== "object") {
    return "Unknown error";
  }
  switch (error.code) {
    case "ENOENT":
      return "No such file or directory";
    case "EACCES":
      return "Permission denied";
    case "EISDIR":
      return "Is a directory";
    case "ENOTDIR":
      return "Not a directory";
    default:
      return error.message || "Unknown error";
  }
}

function makeError(code, message) {
  const error = new Error(message);

  error.code = code;
  return error;
}

function escapeRegExp(text) {
  return text.replace(/[\\^$.*+?()[\]{}|]/g, "\\$&");
}

function buildPatternSource(pattern, options) {
  const body = options.fixed ? escapeRegExp(pattern) : pattern;
  if (options.lineRegexp) {
    return `^(?:${body})$`;
  }
  return body;
}

function compileMatchers(options) {
  const flags = options.ignoreCase ? "i" : "";
  const matchers = [];

  for (const pattern of options.patterns) {
    try {
      matchers.push(new RegExp(buildPatternSource(pattern, options), flags));
    } catch {
      fail(`invalid regex '${pattern}': Invalid argument`);
    }
  }
  return matchers;
}

function parseArgs(argv) {
  const options = {
    fixed: false,
    ignoreCase: false,
    invert: false,
    lineNumbers: false,
    countOnly: false,
    quiet: false,
    forceFilename: false,
    noFilename: false,
    suppressErrors: false,
    lineRegexp: false,
    recursive: false,
    recursiveFollow: false,
    patterns: [],
    files: [],
  };
  let parseOptions = true;

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];

    if (parseOptions && arg === "--") {
      parseOptions = false;
      continue;
    }
    if (parseOptions && arg.startsWith("-") && arg !== "-") {
      if (arg === "-e") {
        i++;
        if (i >= argv.length) {
          usage();
        }
        options.patterns.push(argv[i]);
        continue;
      }
      for (let j = 1; j < arg.length; j++) {
        const flag = arg[j];
        if (flag === "e") {
          if (j + 1 < arg.length) {
            options.patterns.push(arg.slice(j + 1));
          } else {
            i++;
            if (i >= argv.length) {
              usage();
            }
            options.patterns.push(argv[i]);
          }
          break;
        }
        if (flag === "F") {
          options.fixed = true;
        } else if (flag === "i") {
          options.ignoreCase = true;
        } else if (flag === "v") {
          options.invert = true;
        } else if (flag === "n") {
          options.lineNumbers = true;
        } else if (flag === "c") {
          options.countOnly = true;
        } else if (flag === "q") {
          options.quiet = true;
        } else if (flag === "H") {
          options.forceFilename = true;
        } else if (flag === "h") {
          options.noFilename = true;
        } else if (flag === "s") {
          options.suppressErrors = true;
        } else if (flag === "x") {
          options.lineRegexp = true;
        } else if (flag === "r") {
          options.recursive = true;
        } else if (flag === "R") {
          options.recursive = true;
          options.recursiveFollow = true;
        } else {
          usage();
        }
      }
      continue;
    }
    if (options.patterns.length === 0) {
      options.patterns.push(arg);
    } else {
      options.files.push(arg);
    }
  }

  if (options.patterns.length === 0) {
    usage();
  }
  if (options.files.length === 0) {
    options.files.push("-");
  }
  return options;
}

function readSource(path) {
  if (path === "-") {
    return fs.readFileSync(0, "utf8");
  }
  return runtimeFs.readTextFileSync(path);
}

function splitLines(text) {
  const lines = [];
  let start = 0;

  for (let i = 0; i < text.length; i++) {
    if (text.charCodeAt(i) === 10) {
      lines.push(text.slice(start, i));
      start = i + 1;
    }
  }
  if (start < text.length) {
    lines.push(text.slice(start));
  }
  return lines;
}

function sourceLabel(path) {
  return path === "-" ? STDIN_LABEL : path;
}

function expandSources(options, onError) {
  const sources = [];
  const visitedRealpaths = new Set();

  function report(path, error) {
    if (onError) {
      onError(path, error);
    }
  }

  function pushSource(path) {
    sources.push(path);
  }

  function walk(path, nested) {
    let info;

    if (path === "-") {
      pushSource(path);
      return;
    }

    try {
      info = runtimeFs.classifyPathSync(path);
    } catch (error) {
      report(path, error);
      return;
    }

    const effectiveKind = info.isSymlink ? info.targetKind : info.kind;

    if (effectiveKind === "directory") {
      if (!options.recursive) {
        report(path, makeError("EISDIR", "Is a directory"));
        return;
      }
      if (info.isSymlink && !options.recursiveFollow) {
        if (!nested) {
          report(path, makeError("EISDIR", "Is a directory"));
        }
        return;
      }

      if (options.recursiveFollow) {
        let resolved;

        try {
          resolved = runtimeFs.realpathSync(path);
        } catch (error) {
          report(path, error);
          return;
        }
        if (visitedRealpaths.has(resolved)) {
          return;
        }
        visitedRealpaths.add(resolved);
      }

      let entries;

      try {
        entries = runtimeFs.listDirSync(path);
      } catch (error) {
        report(path, error);
        return;
      }

      for (const entry of entries) {
        const entryKind = entry.isSymlink ? entry.targetKind : entry.kind;

        if (entryKind === "directory") {
          if (entry.isSymlink && !options.recursiveFollow) {
            continue;
          }
          walk(entry.path, true);
        } else {
          pushSource(entry.path);
        }
      }
      return;
    }

    pushSource(path);
  }

  for (const path of options.files) {
    walk(path, false);
  }

  return sources;
}

function processText(text, label, showFilename, options, matchers) {
  const lines = splitLines(text);
  let selectedCount = 0;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    const matched = matchers.some((matcher) => matcher.test(line));
    const selected = options.invert ? !matched : matched;

    if (!selected) {
      continue;
    }
    selectedCount++;
    if (options.quiet) {
      return { selectedCount, quietMatch: true };
    }
    if (!options.countOnly) {
      let prefix = "";

      if (showFilename) {
        prefix += `${label}:`;
      }
      if (options.lineNumbers) {
        prefix += `${i + 1}:`;
      }
      process.stdout.write(prefix + line + "\n");
    }
  }

  if (options.countOnly && !options.quiet) {
    const prefix = showFilename ? `${label}:` : "";
    process.stdout.write(prefix + String(selectedCount) + "\n");
  }

  return { selectedCount, quietMatch: false };
}

function main() {
  const options = parseArgs(process.argv.slice(2));
  const matchers = compileMatchers(options);
  let hadSelected = false;
  let hadError = false;
  const sources = expandSources(options, (path, error) => {
    hadError = true;
    if (!options.suppressErrors) {
      process.stderr.write(`${PROGRAM}: ${path}: ${errorText(error)}\n`);
    }
  });
  const showFilename =
    options.noFilename ? false : options.forceFilename ? true : sources.length > 1;

  for (const path of sources) {
    let text;

    try {
      text = readSource(path);
    } catch (error) {
      hadError = true;
      if (!options.suppressErrors) {
        process.stderr.write(`${PROGRAM}: ${path}: ${errorText(error)}\n`);
      }
      continue;
    }

    const result = processText(
      text,
      sourceLabel(path),
      showFilename,
      options,
      matchers,
    );

    if (result.selectedCount > 0) {
      hadSelected = true;
    }
    if (result.quietMatch) {
      process.exit(0);
    }
  }

  if (hadError) {
    process.exit(2);
  }
  process.exit(hadSelected ? 0 : 1);
}

main();

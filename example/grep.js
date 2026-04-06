const fs = require("fs");

const PROGRAM = process.argv[1]
  ? process.argv[1].split(/[\\/]/).pop()
  : "grep.js";
const STDIN_LABEL = "(standard input)";

function usage() {
  process.stderr.write(
    `usage: ${PROGRAM} [-FivncqHhsx] [-e pattern] [pattern] [file ...]\n`,
  );
  process.exit(2);
}

function fail(message) {
  process.stderr.write(`${PROGRAM}: ${message}\n`);
  process.exit(2);
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

  try {
    return options.patterns.map(
      (pattern) => new RegExp(buildPatternSource(pattern, options), flags),
    );
  } catch (error) {
    fail(`invalid regex: ${error.message}`);
  }
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
  return fs.readFileSync(path, "utf8");
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

function shouldShowFilename(options) {
  if (options.noFilename) {
    return false;
  }
  if (options.forceFilename) {
    return true;
  }
  return options.files.length > 1;
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
  const showFilename = shouldShowFilename(options);
  let hadSelected = false;
  let hadError = false;

  for (const path of options.files) {
    let text;

    try {
      text = readSource(path);
    } catch (error) {
      hadError = true;
      if (!options.suppressErrors) {
        process.stderr.write(`${PROGRAM}: ${path}: ${error.message}\n`);
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

# Program Shape

The default output is one standalone C file with a production-style `main(...)`.

## Required Structure

- include only the C and `jsmx` headers the program actually uses
- prefer:
  - `#include <errno.h>`
  - `#include <stdio.h>`
  - `#include <stdlib.h>`
  - `#include <string.h>`
  - `#include "jsval.h"`
  - `#include "jsmethod.h"`
  - `#include "jsregex.h"` only when regex is needed
- use `main(int argc, char **argv)` by default
- place small `static` helper functions above `main(...)` for:
  - repeated string copying
  - stdin/file slurp helpers
  - explicit output formatting
  - repeated array/object bridge helpers

## Header Comment

Put a short comment block near the top with:

- source JS entrypoint path
- intended behavior in one or two lines
- lowering class:
  - `production_flattened`
  - `production_slow_path`
  - `manual_runtime_needed`
- any required host assumptions

## Region And Buffer Policy

This skill is for production binaries, not compliance fixtures. Heap allocation is acceptable when it stays explicit and improves usability.

Preferred policy:

- keep `jsval_region_t` ownership explicit
- allocate one region buffer explicitly when the program's working set can be estimated conservatively
- allocate I/O buffers explicitly for stdin or file reads when the JS program needs text input
- free heap-backed host buffers before exit when practical

Do not hide allocation or lifetime decisions inside opaque helper layers.

If the program's semantics require unbounded growth with no honest memory plan, stop and mark it `manual_runtime_needed`.

## Compile Commands

Prefer explicit compile commands over build-system indirection.

Non-regex default:

```sh
cc -I. generated_program.c \
  jsmn.c jsnum.c jsval.c jsmethod.c jsregex.c jsstr.c unicode.c jsurl.c utf8.c \
  -o generated_program
```

Regex-enabled variant:

```sh
cc -I. -DJSMX_WITH_REGEX=1 -DJSMX_REGEX_BACKEND_PCRE2=1 generated_program.c \
  jsmn.c jsnum.c jsval.c jsmethod.c jsregex.c jsstr.c unicode.c jsurl.c utf8.c \
  -lpcre2-16 \
  -o generated_program
```

If the local PCRE2 install needs extra include or library search paths, spell them out in the generated command instead of hiding them.

## Validation

When the program stays inside the supported boundary, validate by:

1. compiling the generated C program
2. running the original JS with Node on representative inputs
3. running the generated binary on the same inputs
4. comparing stdout, stderr, and exit status

If exact parity depends on unsupported host/runtime behavior, stop and report that instead of claiming production equivalence.

## Validation Targets

Use these three canonical checks when validating the skill itself:

1. CLI argument script
   - a JS entrypoint that reads `process.argv`, performs string/value work, and writes a final line to stdout
   - expected outcome: `production_flattened` or `production_slow_path`, plus a working C binary
2. stdin-to-stdout transformer
   - a JS entrypoint that slurps text input, runs current `jsmx` string/value helpers, and prints transformed output
   - expected outcome: direct program generation with explicit stdin bridge code
3. boundary-crossing host script
   - a JS entrypoint that depends on async timers, dynamic modules, or unsupported Node host behavior
   - expected outcome: `manual_runtime_needed`, with a precise report naming the missing runtime surface

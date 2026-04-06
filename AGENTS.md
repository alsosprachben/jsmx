# Repository Guidelines

## Project Structure & Module Organization

This repository is a C runtime support library for transpiling JavaScript to C.

### Core modules

- `jsmn.c`, `jsmn.h`
  - parser core and token model
- `jsnum.c`, `jsnum.h`
  - numeric text / math helpers
  - shared by `jsval`
- `jsregex.c`, `jsregex.h`, `jsmx_config.h`
  - optional backend-gated regex support
- top-level JS-facing helpers
  - `jsval.c`
  - `jsmethod.c`
  - `jsstr.c`
  - `unicode.c`
  - `mnurl.c`
  - `utf8.h`

### `jsval`

`jsval` now provides:

- versioned page-set storage with an in-page root handle
  - relocated buffers remain self-describing
- readable JSON-backed values without implicit mutation
- explicit promotion helpers for generated C
- shallow capacity-planned promotion
  - the translator can selectively convert only the object or array that will
    mutate
- JSON emission for JSON-compatible values
- explicit native object / array helpers for:
  - own-property checks
  - ordered key / value access
  - shallow own-property copy
  - fresh own-property clone into a new native object
  - fresh dense-array clone into a new native array
  - bounded dense-array splice edits with removed-result arrays
  - translator-visible object / array spread-style lowerings built from
    clone, copy, and append recipes
  - delete
  - overwrite
  - push
  - pop
  - shift
  - unshift
  - dense length writes
- primitive value helpers plus:
  - `typeof`
  - nullish detection
  - arithmetic
  - integer coercion
  - bitwise operators
  - shift operators
  - abstract equality
  - relational helpers
- flattened generated-code support for:
  - feature-gated native regex values
  - semantic `source`
  - `lastIndex`
  - `exec`
  - `test`
  - `match`
  - `matchAll`
  - flag / state helpers
  - deeper repeated exec-result and `lastIndex` fidelity
  - named capture groups and `groups` objects on exec-shaped results and
    replace-family substitutions
  - explicit translator-visible JIT versus non-JIT regex construction, with
    JIT intended only for clearly hoisted reused direct-lowered regexes
- translator-facing callback replacers for `replace` / `replaceAll`,
  including regex named-group `groups` arguments
- narrow `/u` literal rewrite helpers for regex-backed string methods,
  including:
  - lone-surrogate atoms
  - fixed no-capture sequences
  - simple no-capture literal-member classes
  - simple no-capture negated literal-member classes
  - simple no-capture ranged classes
  - simple no-capture negated ranged classes
  - simple no-capture predefined classes `\d`, `\D`, `\s`, `\S`, `\w`, and
    `\W`
  - `matchAll`
- a direct bridge from `jsval_t` into the thin string-method layer

### `jsmethod`

`jsmethod` is the thin JS-method layer for:

- receiver coercion
- Symbol rejection
- typed method errors
- string built-ins such as:
  - `normalize`
  - `concat`
  - `replace`
  - `replaceAll`
  - case conversion
  - trim / repeat plus the legacy trim aliases
  - padding:
    - `padStart`
    - `padEnd`
  - non-regex search / membership:
    - `indexOf`
    - `lastIndexOf`
    - `includes`
    - `startsWith`
    - `endsWith`
    - `split`
  - feature-gated regex-backed `search`
  - accessor methods:
    - `charAt`
    - `at`
    - `charCodeAt`
    - `codePointAt`
  - substring extraction:
    - `slice`
    - `substring`
    - `substr`
  - `toWellFormed`

### Unicode and normalization

- `jsstr`
  - explicit normalization-form APIs for:
    - `NFC`
    - `NFD`
    - `NFKC`
    - `NFKD`
  - legacy `jsstr*_normalize()` entrypoints remain NFC wrappers
- `unicode`
  - exposes a small parser for mapping JS form strings onto that enum surface

The normalization path is explicitly two-phase all the way up the stack.

Callers can measure exact workspace and result sizes with:

- `unicode_normalize_form_workspace_len()`
- `unicode_normalize_form_needed()`
- `jsstr*_normalize_form_needed()`

Then generated code can use:

- `jsmethod_string_normalize_measure()`
- `jsmethod_string_normalize_into()`
- `jsval_method_string_normalize_measure()`
- `jsval_method_string_normalize()`

The same measure-then-execute pattern now also covers:

- concat
  - `jsmethod_string_concat_measure()`
  - `jsmethod_string_concat()`
- replace
  - `jsmethod_string_replace_measure()`
  - `jsmethod_string_replace()`
- replaceAll
  - `jsmethod_string_replace_all_measure()`
  - `jsmethod_string_replace_all()`
- repeat
  - `jsmethod_string_repeat_measure()`
  - `jsmethod_string_repeat()`
- padding
  - `jsmethod_string_pad_start_measure()`
  - `jsmethod_string_pad_start()`
  - `jsmethod_string_pad_end_measure()`
  - `jsmethod_string_pad_end()`
- matching `jsval` bridge entrypoints

### Repository layout

- examples live in `example/`
- reusable host-module bridges for transpiled production programs live in
  `runtime_modules/`
  - shared implementations belong under `runtime_modules/shared/`
  - runtime exposure profiles belong under paths such as
    `runtime_modules/node/` and `runtime_modules/wintertc/`
- parser regression tests live in
  [`test/tests.c`](/home/ben/repos/jsmx/test/tests.c)
- feature-specific executables use files such as:
  - `test_jsnum.c`
  - `test_jsregex.c`
  - `test_jsval.c`
  - `test_jsmethod.c`
  - `test_codegen.c`
  - `test_jsstr.c`
  - `test_unicode.c`
  - `test_mnurl.c`
- real compliance sources and committed generated fixtures live under
  `compliance/`
  - suites:
    - `strings`
    - `regex`
    - `values`
    - `objects`
- repo-local translation workflow guidance lives in
  `skills/jsmx-transpile-tests/`
- the flattening boundary is documented in
  [`docs/flattening-boundary.md`](/home/ben/repos/jsmx/docs/flattening-boundary.md)
- arrays in the current flattened layer are:
  - dense
  - capacity-bounded
  - still out of scope for hole and sparse-index semantics
- Unicode source data and generated headers also live at the repository root:
  - `UnicodeData.txt`
  - `allkeys.txt`
  - `unicode_*.h`
- generator scripts live in `scripts/`

### Current compliance coverage

- strings
  - official concat / replace / replaceAll coverage
  - named-group regex replace / replaceAll parity coverage
  - official trim / repeat coverage
  - runtime coverage for legacy trim aliases:
    - `trimLeft`
    - `trimRight`
  - padding coverage:
    - `padStart`
    - `padEnd`
  - search-core coverage:
    - `indexOf`
    - `lastIndexOf`
    - `includes`
    - `startsWith`
    - `endsWith`
    - `split`
  - feature-gated regex coverage:
    - `search`
    - `replace`
    - `replaceAll`
    - `split`
    - `exec`
    - `match`
    - `matchAll`
  - accessor coverage:
    - `charAt`
    - `at`
    - `charCodeAt`
    - `codePointAt`
  - substring extraction coverage:
    - `slice`
    - `substring`
    - `substr`
  - includes idiomatic slow-path translations where abrupt coercion or known
    object-literal coercions cross the flattened boundary
- regex
  - feature-gated semantic `RegExp.prototype.source`
  - `RegExp.prototype.test`
  - `RegExp.prototype[Symbol.matchAll]`
  - supported flag / state access
  - constructor-state behavior
  - deeper repeated `exec` / `test` / `match` state fidelity
  - repo-authored parsed / native parity for the current regex core
- values
  - primitive `typeof`
  - nullish-coalescing parity
  - ternary truthiness / identity parity
  - official test262 strict-equality
  - primitive abstract equality / inequality
  - logical-operator slices:
    - `!`
    - `&&`
    - `||`
  - primitive numeric coercion / arithmetic
  - primitive integer coercion plus bitwise and shift slices:
    - `~`
    - `&`
    - `|`
    - `^`
    - `<<`
    - `>>`
    - `>>>`
  - primitive relational-operator slices across numeric and string ordering
    paths:
    - `<`
    - `<=`
    - `>`
    - `>=`
  - idiomatic slow-path translations for official value files whose
    boxed-wrapper checks are already understood above the flattened boundary

## Build, Test, and Development Commands

Use `make` to build the library, examples, and all test binaries.

### Common targets

- `make`
  - builds `libjsmn.a`, example binaries, and all tests
- `make test`
  - runs the parser matrix in `test/`
  - variants:
    - `default`
    - `strict`
    - `links`
    - `strict_links`
    - `emitter`
- `make test_jsnum`
  - builds and runs the internal numeric backend tests that back `jsval`
    parsing, formatting, remainder, and integer-coercion semantics
- `make test_jsval`
  - builds and runs the page-resident value / object storage tests
  - includes native object / array helper coverage and the `jsval` to
    `jsmethod` bridge
- `make test_jsmethod`
  - builds and runs the thin JS-method coercion and normalize tests
- `make test_codegen`
  - builds and runs the generated-C smoke harness across value, native
    object / array, and string semantics
- `make test_compliance`
  - builds and runs committed generated compliance fixtures listed in
    `compliance/manifest.json` in parallel
  - the runner builds one shared runtime archive per invocation
  - then compiles each fixture against it
  - `JOBS=N` or `COMPLIANCE_JOBS=N` override worker count
- `make bench`
  - runs the dedicated benchmark corpus in `bench/` against both Node and
    compiled generated C, writing local JSON timing results by default
- `make test_jsstr`, `make test_unicode`, `make test_mnurl`, `make test_utf8`,
  `make test_collation`
  - build and run focused feature tests
- `make clean`
  - removes common build artifacts
  - check `git status` afterward because some generated test binaries may
    remain

If you need custom compiler flags, add a local `config.mk` rather than editing
`Makefile`.

## Coding Style & Naming Conventions

Match the existing C style:

- tabs for indentation
- opening braces on the same line
- short `/* ... */` comments only where logic is non-obvious

Naming:

- `snake_case` for functions and helpers
- `UPPER_CASE` for macros
- pair new public modules as `name.c` and `name.h`

General guidance:

- prefer C-idiomatic interfaces that preserve JavaScript semantics
- do not mirror JS syntax directly unless there is a strong reason
- there is no repo-managed formatter or linter
  - preserve local include order and whitespace patterns

## Testing Guidelines

There is no formal coverage gate, but new behavior should ship with a targeted
test.

### Where tests go

- add parser cases to `test/tests.c`
- add standalone feature tests as `test_<area>.c` at the repo root when they
  need separate fixtures or generated Unicode data

### Compliance fixture guidance

- prefer a real upstream source under `compliance/js/`
- pair it with one idiomatic generated C fixture under `compliance/generated/`
- keep repo-authored sources for `jsmx`-specific contracts such as:
  - JSON-backed parity
  - planned-capacity behavior
  - legacy alias behavior when the upstream directory only contains metadata or
    same-function-object checks

### Manifest metadata

Update `compliance/manifest.json` and set:

- `lowering_class`
  - `static_pass`
  - `slow_path_needed`
  - `unsupported`
- `translation_mode`
  - `idiomatic_flattened`
  - `idiomatic_slow_path`
  - `literal`

Use [`docs/flattening-boundary.md`](/home/ben/repos/jsmx/docs/flattening-boundary.md)
as the boundary reference.

Preferred choices:

- prefer passing idiomatic slow-path fixtures over boundary-only
  `KNOWN_UNSUPPORTED` placeholders when the slow-path contract is already clear
- prefer idiomatic translations over literal ones unless the literal structure
  itself needs review

For regex-bearing fixtures:

- decide direct lowering, rewrite-backed lowering, idiomatic slow path, or
  unsupported status in the translator guidance first
- make new or updated manifest notes start with:
  - `Direct-lowered:`
  - `Rewrite-backed:`
  - `Idiomatic slow path:`
  - `Unsupported:`

`make test_compliance` is manifest-driven, so new committed fixtures should
become runnable by updating the manifest rather than editing `Makefile`.

Numeric internals now have both default and `USE_LIBC` backends. When you touch
`jsnum` or numeric `jsval` behavior, verify both:

- `make test_jsnum test_jsval test_codegen test_compliance test`
- `make CFLAGS='-DUSE_LIBC' LDLIBS='-lm' test_jsnum test_jsval test_codegen test_compliance test`

Run the narrowest relevant target first, then `make test` before opening a PR.

## Commit & Pull Request Guidelines

Recent history favors short imperative subjects, for example:

- `Implement NFC normalization`
- `Refactor normalize API`

Guidance:

- keep commits focused
- explain regenerated data files explicitly when Unicode headers change

Pull requests should include:

- a brief problem statement
- the main files touched
- the exact test commands you ran

Screenshots are not relevant here. `git status` should be clean of build
outputs before review.
